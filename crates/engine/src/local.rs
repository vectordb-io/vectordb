//! Single-process filesystem adapter for the native segment and query engines.
use crate::{
    Error, Result,
    schema::name,
    segment::SegmentEngine,
    segment_storage::{StateManifest, checksum},
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    collections::{BTreeMap, BTreeSet},
    fs::{self, File, OpenOptions},
    io::Write,
    path::{Path, PathBuf},
};

const FORMAT: &[u8] = b"VectorDB local segments v1\n";

#[derive(Default, Clone, Serialize, Deserialize)]
struct Names {
    databases: BTreeSet<String>,
    aliases: BTreeMap<String, String>,
}
#[derive(Clone)]
struct State {
    names: Names,
    collections: BTreeMap<String, SegmentEngine>,
    revision: i64,
}
impl Default for State {
    fn default() -> Self {
        Self {
            names: Names {
                databases: BTreeSet::from(["default".into()]),
                aliases: BTreeMap::new(),
            },
            collections: BTreeMap::new(),
            revision: 0,
        }
    }
}
#[derive(Serialize, Deserialize)]
struct Head {
    revision: i64,
    names: Names,
    collections: BTreeMap<String, StateManifest>,
}
#[derive(Serialize, Deserialize)]
struct Record {
    revision: i64,
    operation: String,
    request: Value,
}

/// The process lock protects the local catalog, WAL and snapshot publication together.
pub struct LocalStore {
    root: PathBuf,
    _lock: File,
    state: State,
    checkpoint: i64,
    failed: bool,
}

fn checked<T: Serialize>(value: &T) -> Result<Vec<u8>> {
    let payload = serde_json::to_string(value)?;
    Ok(serde_json::to_vec(&(
        checksum(payload.as_bytes()),
        payload,
    ))?)
}
fn decode<T: serde::de::DeserializeOwned>(path: &Path) -> Result<T> {
    let (digest, payload): (String, String) = serde_json::from_reader(File::open(path)?)?;
    if checksum(payload.as_bytes()) != digest {
        return Err(Error::Integrity("local file checksum mismatch".into()));
    }
    Ok(serde_json::from_str(&payload)?)
}
fn sync_dir(path: &Path) -> Result<()> {
    File::open(path)?.sync_all()?;
    Ok(())
}
fn sync_parent(path: &Path) -> Result<()> {
    sync_dir(
        path.parent()
            .filter(|p| !p.as_os_str().is_empty())
            .unwrap_or_else(|| Path::new(".")),
    )
}
fn write_file(path: &Path, bytes: &[u8]) -> Result<()> {
    let mut f = File::create(path)?;
    f.write_all(bytes)?;
    f.sync_all()?;
    Ok(())
}
fn atomic(path: &Path, bytes: &[u8]) -> Result<()> {
    let temp = path.with_extension("tmp");
    write_file(&temp, bytes)?;
    fs::rename(&temp, path)?;
    sync_dir(path.parent().unwrap())
}
fn text<'a>(r: &'a Value, field: &str) -> Result<&'a str> {
    let value = r[field]
        .as_str()
        .ok_or_else(|| Error::Invalid(format!("{field} required")))?;
    name(value)?;
    Ok(value)
}
fn full(db: &str, collection: &str) -> String {
    format!("{db}/{collection}")
}
fn mutate(op: &str) -> bool {
    matches!(
        op,
        "databases/create"
            | "databases/drop"
            | "aliases/create"
            | "aliases/alter"
            | "aliases/drop"
            | "collections/create"
            | "collections/drop"
            | "partitions/create"
            | "partitions/drop"
            | "indexes/create"
            | "indexes/drop"
            | "entities/insert"
            | "entities/upsert"
            | "entities/delete"
    )
}
impl State {
    fn canonical(&self, db: &str, c: &str) -> Result<String> {
        let key = full(db, c);
        let key = self.names.aliases.get(&key).unwrap_or(&key);
        self.collections
            .contains_key(key)
            .then(|| key.to_owned())
            .ok_or_else(|| Error::NotFound("collection not found".into()))
    }
    fn apply(&mut self, op: &str, mut req: Value, revision: i64) -> Result<Value> {
        // The segment executor validates its extended create/delete contracts.
        if let Some(action) = op.strip_prefix("indexes/")
            && matches!(action, "build" | "retry" | "cancel" | "get_status")
        {
            crate::validate_request("collections/describe", &req)?;
            let db = req
                .get("dbName")
                .map(|_| text(&req, "dbName"))
                .transpose()?
                .unwrap_or("default");
            let key = self.canonical(db, text(&req, "collectionName")?)?;
            let e = &self.collections[&key];
            match action {
                "build" | "retry" => e.build_indexes()?,
                "cancel" => e.cancel_index_builds(),
                _ => (),
            }
            return Ok(e.index_status());
        }
        if !matches!(op, "collections/create" | "entities/delete") {
            crate::validate_request(op, &req)?;
        }
        let db = req
            .get("dbName")
            .map(|_| text(&req, "dbName"))
            .transpose()?
            .unwrap_or("default")
            .to_owned();
        if op != "databases/create" && !self.names.databases.contains(&db) {
            return Err(Error::NotFound("database not found".into()));
        }
        match op {
            "databases/create" => {
                if !self.names.databases.insert(db) {
                    return Err(Error::Conflict("database exists".into()));
                }
                return Ok(json!({}));
            }
            "databases/drop" => {
                if db == "default"
                    || self
                        .collections
                        .keys()
                        .any(|k| k.starts_with(&format!("{db}/")))
                {
                    return Err(Error::Conflict(
                        "database must be empty and non-default".into(),
                    ));
                }
                self.names.databases.remove(&db);
                return Ok(json!({}));
            }
            "databases/list" => return Ok(json!(self.names.databases)),
            "databases/describe" => return Ok(json!({"dbName":db})),
            "collections/list" => {
                return Ok(json!(
                    self.collections
                        .keys()
                        .filter_map(|k| k.strip_prefix(&format!("{db}/")))
                        .collect::<Vec<_>>()
                ));
            }
            "collections/has" => {
                return Ok(json!({"has":self.canonical(&db,text(&req,"collectionName")?).is_ok()}));
            }
            "aliases/drop" | "aliases/describe" => {
                let alias = text(&req, "aliasName")?;
                let key = full(&db, alias);
                let target = self
                    .names
                    .aliases
                    .get(&key)
                    .ok_or_else(|| Error::NotFound("alias not found".into()))?;
                if op == "aliases/describe" {
                    return Ok(
                        json!({"dbName":db,"aliasName":alias,"collectionName":target.split_once('/').unwrap().1}),
                    );
                }
                self.names.aliases.remove(&key);
                return Ok(json!({}));
            }
            _ => (),
        }
        let n = text(&req, "collectionName")?.to_owned();
        let key = full(&db, &n);
        if op == "collections/create" {
            if self.collections.contains_key(&key) || self.names.aliases.contains_key(&key) {
                return Err(Error::Conflict("name already exists".into()));
            }
            req["dbName"] = json!("default");
            let mut engine = SegmentEngine::new();
            let batch = engine.prepare(op, req)?;
            let result = engine.commit(batch, revision)?;
            self.collections.insert(key, engine);
            return Ok(result);
        }
        let canonical = self.canonical(&db, &n)?;
        match op {
            "aliases/create" | "aliases/alter" => {
                let key = full(&db, text(&req, "aliasName")?);
                if self.collections.contains_key(&key)
                    || self.names.aliases.contains_key(&key) != (op == "aliases/alter")
                {
                    return Err(Error::Conflict("alias name unavailable".into()));
                }
                self.names.aliases.insert(key, canonical);
                return Ok(json!({}));
            }
            "aliases/list" => {
                return Ok(json!(
                    self.names
                        .aliases
                        .iter()
                        .filter(|(_, target)| *target == &canonical)
                        .map(|(alias, _)| alias.split_once('/').unwrap().1)
                        .collect::<Vec<_>>()
                ));
            }
            "collections/drop" => {
                if key != canonical {
                    return Err(Error::Invalid(
                        "drop requires canonical collection name".into(),
                    ));
                }
                self.names.aliases.retain(|_, v| v != &key);
                self.collections.remove(&key);
                return Ok(json!({}));
            }
            _ => (),
        }
        req["dbName"] = json!("default");
        req["collectionName"] = json!(canonical.split_once('/').unwrap().1);
        let engine = self.collections.get_mut(&canonical).unwrap();
        if mutate(op) {
            let batch = engine.prepare(op, req)?;
            return engine.commit(batch, revision);
        }
        engine.read_view()?.execute(op, req)
    }
    fn snapshot(&self, root: &Path) -> Result<()> {
        let dir = root.join(format!("snapshot-{}", self.revision));
        // This revision is unpublished. An interrupted previous attempt may have left files.
        fs::create_dir_all(&dir)?;
        let mut head = Head {
            revision: self.revision,
            names: self.names.clone(),
            collections: BTreeMap::new(),
        };
        for (id, engine) in &self.collections {
            engine.build_indexes()?;
            let sealed = engine.seal()?;
            let objects = dir.join(checksum(id.as_bytes()));
            fs::create_dir_all(&objects)?;
            for (key, bytes) in sealed.objects {
                write_file(&objects.join(key), &bytes)?;
            }
            sync_dir(&objects)?;
            head.collections.insert(id.clone(), sealed.manifest);
        }
        sync_dir(&dir)?;
        atomic(&root.join("HEAD"), &checked(&head)?)
    }
}
impl LocalStore {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let root = path.as_ref().to_owned();
        fs::create_dir_all(&root)?;
        let lock = OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .open(root.join(".lock"))?;
        fs2::FileExt::try_lock_exclusive(&lock)
            .map_err(|_| Error::Conflict("local store already open".into()))?;
        let marker = root.join("FORMAT");
        if !marker.exists() {
            for entry in fs::read_dir(&root)? {
                if entry?.file_name() != ".lock" {
                    return Err(Error::Invalid(
                        "expected an empty native store directory".into(),
                    ));
                }
            }
            State::default().snapshot(&root)?;
            atomic(&marker, FORMAT)?;
            sync_parent(&root)?;
        }
        if fs::read(&marker)? != FORMAT {
            return Err(Error::Integrity("unsupported local store format".into()));
        }
        fs::create_dir_all(root.join("wal"))?;
        sync_dir(&root)?;
        let mut state = State::default();
        if !root.join("HEAD").is_file() {
            return Err(Error::Integrity("local store HEAD missing".into()));
        }
        {
            let head: Head = decode(&root.join("HEAD"))?;
            state.names = head.names;
            state.revision = head.revision;
            for (key, manifest) in head.collections {
                let objects = root
                    .join(format!("snapshot-{}", head.revision))
                    .join(checksum(key.as_bytes()));
                let engine = SegmentEngine::load(manifest, |key| {
                    if Path::new(key).file_name().and_then(|p| p.to_str()) != Some(key) {
                        return Err(Error::Integrity("invalid local object name".into()));
                    }
                    Ok(fs::read(objects.join(key))?)
                })?;
                state.collections.insert(key, engine);
            }
        }
        let checkpoint = state.revision;
        let mut records = BTreeMap::new();
        for file in fs::read_dir(root.join("wal"))? {
            let path = file?.path();
            if path.extension().is_some_and(|e| e == "tmp") {
                continue;
            }
            let n = path
                .file_name()
                .and_then(|f| f.to_str())
                .and_then(|s| s.parse::<i64>().ok())
                .ok_or_else(|| Error::Integrity("invalid local WAL name".into()))?;
            if n > checkpoint {
                records.insert(n, path);
            }
        }
        for (n, path) in records {
            let rec: Record = decode(&path)?;
            if n != state.revision + 1 || rec.revision != n || !mutate(&rec.operation) {
                return Err(Error::Integrity("invalid local WAL sequence".into()));
            }
            state.apply(&rec.operation, rec.request, n)?;
            state.revision = n;
        }
        Ok(Self {
            root,
            _lock: lock,
            state,
            checkpoint,
            failed: false,
        })
    }
    pub fn execute(&mut self, op: &str, req: Value) -> Result<Value> {
        if self.failed {
            return Err(Error::Integrity(
                "local commit outcome unknown; reopen store".into(),
            ));
        }
        if matches!(op, "collections/compact" | "collections/flush") {
            crate::validate_request(op, &req)?;
            let db = req
                .get("dbName")
                .map(|_| text(&req, "dbName"))
                .transpose()?
                .unwrap_or("default");
            self.state.canonical(db, text(&req, "collectionName")?)?;
            self.flush()?;
            return Ok(json!({}));
        }
        if !mutate(op) {
            return self.state.apply(op, req, self.state.revision);
        }
        if self.state.revision - self.checkpoint >= 128 {
            self.flush()?;
        }
        let n = self
            .state
            .revision
            .checked_add(1)
            .ok_or_else(|| Error::Capacity("local revision exhausted".into()))?;
        let mut next = self.state.clone();
        let result = next.apply(op, req.clone(), n)?;
        let record = checked(&Record {
            revision: n,
            operation: op.into(),
            request: req,
        })?;
        // A failed fsync after rename has an uncertain outcome; fence this handle.
        self.failed = true;
        atomic(&self.root.join("wal").join(n.to_string()), &record)?;
        next.revision = n;
        self.state = next;
        self.failed = false;
        Ok(result)
    }
    pub fn flush(&mut self) -> Result<()> {
        if self.failed {
            return Err(Error::Integrity(
                "cannot checkpoint uncertain local state".into(),
            ));
        }
        if self.checkpoint != self.state.revision {
            self.failed = true;
            self.state.snapshot(&self.root)?;
            self.failed = false;
            self.checkpoint = self.state.revision;
            for c in self.state.collections.values_mut() {
                c.reclaim_tombstones();
            }
        }
        for entry in fs::read_dir(self.root.join("wal"))? {
            let entry = entry?;
            if entry
                .file_name()
                .to_str()
                .and_then(|s| s.parse::<i64>().ok())
                .is_some_and(|n| n <= self.checkpoint)
            {
                fs::remove_file(entry.path())?;
            }
        }
        for entry in fs::read_dir(&self.root)? {
            let entry = entry?;
            if entry
                .file_name()
                .to_str()
                .and_then(|s| s.strip_prefix("snapshot-"))
                .and_then(|s| s.parse::<i64>().ok())
                .is_some_and(|n| n != self.checkpoint)
            {
                fs::remove_dir_all(entry.path())?;
            }
        }
        Ok(())
    }
    pub fn export_jsonl(
        &self,
        db: &str,
        collection: &str,
        mut writer: impl std::io::Write,
    ) -> Result<usize> {
        let key = self.state.canonical(db, collection)?;
        let mut count = 0;
        for row in self.state.collections[&key].live_rows() {
            serde_json::to_writer(&mut writer, &row.body)?;
            writer.write_all(b"\n")?;
            count += 1;
        }
        writer.flush()?;
        Ok(count)
    }
    pub fn import_jsonl(
        &mut self,
        db: &str,
        collection: &str,
        reader: impl std::io::BufRead,
        upsert: bool,
        batch_size: usize,
    ) -> Result<usize> {
        self.state.canonical(db, collection)?;
        crate::engine::import_batches(reader, batch_size, |rows| {
            self.execute(
                if upsert {
                    "entities/upsert"
                } else {
                    "entities/insert"
                },
                json!({"dbName":db,"collectionName":collection,"data":rows}),
            )
            .map(|_| ())
        })
    }
    pub fn backup(&self, path: impl AsRef<Path>) -> Result<()> {
        if self.failed {
            return Err(Error::Integrity(
                "cannot back up uncertain local state".into(),
            ));
        }
        let path = path.as_ref();
        fs::create_dir(path)?;
        self.state.snapshot(path)?;
        atomic(&path.join("FORMAT"), FORMAT)?;
        sync_parent(path)
    }
}
