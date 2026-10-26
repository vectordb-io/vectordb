use crate::{
    Error, Result,
    error::invalid,
    filter::{Expr, compare, lookup},
    index::{BuiltIndex, Index, ascending, score},
    schema::{Schema, name, number},
};
use fs2::FileExt;
use rusqlite::{Connection, OptionalExtension, params};
use serde_json::{Value, json};
use std::{
    collections::{BTreeMap, BTreeSet, HashMap},
    fs::{File, OpenOptions},
    path::Path,
    time::Duration,
};

pub struct Engine {
    conn: Connection,
    _lock: Option<File>,
    indexes: HashMap<(i64, String), BuiltIndex>,
}
#[derive(Clone)]
struct Collection {
    id: i64,
    name: String,
    schema: Schema,
    loaded: bool,
    indexes: Vec<Index>,
    next_id: i64,
}
impl Engine {
    /// Opens a database. A process lock prevents two engines from sharing stale indexes.
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let path = path.as_ref();
        if let Some(parent) = path.parent().filter(|p| !p.as_os_str().is_empty()) {
            std::fs::create_dir_all(parent)?;
        }
        let mut lock_name = path.as_os_str().to_os_string();
        lock_name.push(".lock");
        let lock = OpenOptions::new()
            .create(true)
            .truncate(false)
            .read(true)
            .write(true)
            .open(lock_name)?;
        lock.try_lock_exclusive()
            .map_err(|_| Error::Conflict("database is already open by another engine".into()))?;
        Self::init(Connection::open(path)?, Some(lock))
    }
    pub fn memory() -> Result<Self> {
        Self::init(Connection::open_in_memory()?, None)
    }
    fn init(conn: Connection, lock: Option<File>) -> Result<Self> {
        conn.busy_timeout(Duration::from_secs(5))?;
        conn.execute_batch(
            "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;",
        )?;
        let version: i64 = conn.query_row("PRAGMA user_version", [], |r| r.get(0))?;
        if version > 1 {
            return Err(Error::Unsupported(format!(
                "database format {version} is newer than supported version 1"
            )));
        }
        conn.execute_batch("BEGIN IMMEDIATE;
            CREATE TABLE IF NOT EXISTS databases(name TEXT PRIMARY KEY);
            INSERT OR IGNORE INTO databases VALUES('default');
            CREATE TABLE IF NOT EXISTS collections(id INTEGER PRIMARY KEY AUTOINCREMENT, db TEXT NOT NULL REFERENCES databases(name), name TEXT NOT NULL, schema_json TEXT NOT NULL, loaded INTEGER NOT NULL DEFAULT 1, indexes_json TEXT NOT NULL DEFAULT '[]', next_id INTEGER NOT NULL DEFAULT 1, UNIQUE(db,name));
            CREATE TABLE IF NOT EXISTS partitions(cid INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE, name TEXT NOT NULL, loaded INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(cid,name));
            CREATE TABLE IF NOT EXISTS entities(cid INTEGER NOT NULL, pk TEXT NOT NULL, partition TEXT NOT NULL, body TEXT NOT NULL, PRIMARY KEY(cid,pk), FOREIGN KEY(cid,partition) REFERENCES partitions(cid,name) ON DELETE CASCADE);
            CREATE INDEX IF NOT EXISTS entity_partition ON entities(cid,partition);
            CREATE TABLE IF NOT EXISTS aliases(db TEXT NOT NULL REFERENCES databases(name), name TEXT NOT NULL, cid INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE, PRIMARY KEY(db,name));
            PRAGMA user_version=1; COMMIT;")?;
        Ok(Self {
            conn,
            _lock: lock,
            indexes: HashMap::new(),
        })
    }
    /// Executes one REST-style operation, atomically, without an HTTP dependency.
    pub fn execute(&mut self, operation: &str, request: Value) -> Result<Value> {
        validate_request(operation, &request)?;
        if operation == "collections/flush" {
            let db = db_name(&request)?;
            self.collection(db, text(&request, "collectionName")?)?;
            self.flush()?;
            return Ok(json!({}));
        }
        if operation == "collections/compact" {
            let db = db_name(&request)?;
            self.collection(db, text(&request, "collectionName")?)?;
            self.conn.execute_batch("VACUUM; PRAGMA optimize;")?;
            return Ok(json!({"state":"Completed"}));
        }
        self.conn.execute_batch("BEGIN IMMEDIATE")?;
        let result = self.dispatch(operation, &request);
        match result {
            Ok(v) => {
                if let Err(e) = self.conn.execute_batch("COMMIT") {
                    let _ = self.conn.execute_batch("ROLLBACK");
                    self.indexes.clear();
                    return Err(e.into());
                }
                Ok(v)
            }
            Err(e) => {
                self.conn.execute_batch("ROLLBACK")?;
                self.indexes.clear();
                Err(e)
            }
        }
    }
    pub fn flush(&self) -> Result<()> {
        let busy: i64 = self
            .conn
            .query_row("PRAGMA wal_checkpoint(FULL)", [], |row| row.get(0))?;
        if busy != 0 {
            return Err(Error::Conflict(
                "checkpoint is blocked by a reader; retry later".into(),
            ));
        }
        Ok(())
    }
    /// Creates a consistent SQLite snapshot. Destination must not already exist.
    pub fn backup(&self, path: impl AsRef<Path>) -> Result<()> {
        let path = path.as_ref();
        let file = OpenOptions::new().write(true).create_new(true).open(path)?;
        drop(file);
        let result = self
            .conn
            .backup("main", path, None::<fn(rusqlite::backup::Progress)>);
        if result.is_err() {
            let _ = std::fs::remove_file(path);
        }
        result?;
        Ok(())
    }
    fn strings(&self, sql: &str, p: impl rusqlite::Params) -> Result<Vec<String>> {
        let mut s = self.conn.prepare(sql)?;
        let rows = s.query_map(p, |r| r.get(0))?;
        Ok(rows.collect::<std::result::Result<_, _>>()?)
    }
    fn database(&self, db: &str) -> Result<()> {
        name(db)?;
        if !self.conn.query_row(
            "SELECT EXISTS(SELECT 1 FROM databases WHERE name=?1)",
            [db],
            |r| r.get::<_, bool>(0),
        )? {
            return Err(Error::NotFound(format!("database {db} not found")));
        }
        Ok(())
    }
    fn collection(&self, db: &str, n: &str) -> Result<Collection> {
        self.database(db)?;
        let result=self.conn.query_row("SELECT id,name,schema_json,loaded,indexes_json,next_id FROM collections WHERE db=?1 AND (name=?2 OR id=(SELECT cid FROM aliases WHERE db=?1 AND name=?2))",params![db,n],|r|Ok((r.get::<_,i64>(0)?,r.get::<_,String>(1)?,r.get::<_,String>(2)?,r.get::<_,bool>(3)?,r.get::<_,String>(4)?,r.get::<_,i64>(5)?))).optional()?;
        let Some((id, name, schema, loaded, indexes, next_id)) = result else {
            return Err(Error::NotFound(format!("collection {n} not found")));
        };
        Ok(Collection {
            id,
            name,
            schema: serde_json::from_str(&schema)?,
            loaded,
            indexes: serde_json::from_str(&indexes)?,
            next_id,
        })
    }
    fn available(&self, db: &str, n: &str) -> Result<()> {
        name(n)?;
        let used:bool=self.conn.query_row("SELECT EXISTS(SELECT 1 FROM collections WHERE db=?1 AND name=?2 UNION ALL SELECT 1 FROM aliases WHERE db=?1 AND name=?2)",params![db,n],|r|r.get(0))?;
        if used {
            Err(Error::Conflict(format!("name {n} already exists")))
        } else {
            Ok(())
        }
    }
    fn save_indexes(&self, c: &Collection) -> Result<()> {
        self.conn.execute(
            "UPDATE collections SET indexes_json=?1 WHERE id=?2",
            params![serde_json::to_string(&c.indexes)?, c.id],
        )?;
        Ok(())
    }
    fn all_rows(&self, c: &Collection) -> Result<Vec<Value>> {
        self.strings("SELECT body FROM entities WHERE cid=?1 ORDER BY pk", [c.id])?
            .into_iter()
            .map(|s| Ok(serde_json::from_str(&s)?))
            .collect()
    }
    fn selected_rows(&self, c: &Collection, r: &Value, require_loaded: bool) -> Result<Vec<Value>> {
        if require_loaded && !c.loaded {
            return Err(Error::Conflict(
                "collection is released; load it first".into(),
            ));
        }
        let partitions = strings(r, "partitionNames")?;
        let mut allowed = BTreeSet::new();
        for p in &partitions {
            self.partition(c, p)?;
            allowed.insert(p.clone());
        }
        if require_loaded {
            let released = self.strings(
                "SELECT name FROM partitions WHERE cid=?1 AND loaded=0",
                [c.id],
            )?;
            if released
                .iter()
                .any(|p| partitions.is_empty() || allowed.contains(p))
            {
                return Err(Error::Conflict(
                    "requested partition is released; load it first".into(),
                ));
            }
        }
        let mut stmt = self
            .conn
            .prepare("SELECT partition,body FROM entities WHERE cid=?1 ORDER BY pk")?;
        let mut result = Vec::new();
        for row in stmt.query_map([c.id], |r| {
            Ok((r.get::<_, String>(0)?, r.get::<_, String>(1)?))
        })? {
            let (p, s) = row?;
            if partitions.is_empty() || allowed.contains(&p) {
                result.push(serde_json::from_str(&s)?);
            }
        }
        Ok(result)
    }
    fn partition(&self, c: &Collection, p: &str) -> Result<()> {
        if self.conn.query_row(
            "SELECT EXISTS(SELECT 1 FROM partitions WHERE cid=?1 AND name=?2)",
            params![c.id, p],
            |r| r.get::<_, bool>(0),
        )? {
            Ok(())
        } else {
            Err(Error::NotFound(format!("partition {p} not found")))
        }
    }
    /// Export a consistent entity stream. Collection schema is managed separately.
    pub fn export_jsonl(
        &self,
        db: &str,
        collection: &str,
        mut writer: impl std::io::Write,
    ) -> Result<usize> {
        let c = self.collection(db, collection)?;
        let mut statement = self
            .conn
            .prepare("SELECT body FROM entities WHERE cid=?1 ORDER BY pk")?;
        let mut count = 0;
        for row in statement.query_map([c.id], |row| row.get::<_, String>(0))? {
            writer.write_all(row?.as_bytes())?;
            writer.write_all(b"\n")?;
            count += 1;
        }
        writer.flush()?;
        Ok(count)
    }
    /// Import JSONL in independent atomic batches. Earlier batches survive a later error.
    pub fn import_jsonl(
        &mut self,
        db: &str,
        collection: &str,
        mut reader: impl std::io::BufRead,
        upsert: bool,
        batch_size: usize,
    ) -> Result<usize> {
        use std::io::{BufRead, Read};
        if batch_size == 0 || batch_size > 10000 {
            return Err(invalid("batch_size must be 1..10000"));
        }
        self.collection(db, collection)?;
        let mut batch = Vec::new();
        let mut count = 0;
        let mut line_number = 0;
        loop {
            let mut line = String::new();
            let bytes = Read::take(&mut reader, 16 * 1024 * 1024 + 1).read_line(&mut line)?;
            if bytes == 0 {
                break;
            }
            line_number += 1;
            if bytes > 16 * 1024 * 1024 {
                return Err(invalid(format!(
                    "line {line_number} exceeds 16 MiB; {count} rows already committed"
                )));
            }
            if line.trim().is_empty() {
                continue;
            }
            let value: Value = serde_json::from_str(&line).map_err(|e| {
                invalid(format!(
                    "JSONL line {line_number}: {e}; {count} rows already committed"
                ))
            })?;
            batch.push(value);
            if batch.len() == batch_size {
                let n = batch.len();
                self.execute(
                    if upsert {
                        "entities/upsert"
                    } else {
                        "entities/insert"
                    },
                    json!({"dbName":db,"collectionName":collection,"data":batch}),
                )?;
                count += n;
                batch = Vec::new();
            }
        }
        if !batch.is_empty() {
            let n = batch.len();
            self.execute(
                if upsert {
                    "entities/upsert"
                } else {
                    "entities/insert"
                },
                json!({"dbName":db,"collectionName":collection,"data":batch}),
            )?;
            count += n;
        }
        Ok(count)
    }
    fn dispatch(&mut self, op: &str, r: &Value) -> Result<Value> {
        let db = db_name(r)?;
        match op {
            "databases/list" => {
                return Ok(json!(
                    self.strings("SELECT name FROM databases ORDER BY name", [])?
                ));
            }
            "databases/create" => {
                let db = text(r, "dbName")?;
                name(db)?;
                if self
                    .conn
                    .execute("INSERT OR IGNORE INTO databases VALUES(?1)", [db])?
                    == 0
                {
                    return Err(Error::Conflict("database already exists".into()));
                }
                return Ok(json!({}));
            }
            "databases/drop" => {
                self.database(db)?;
                if db == "default" {
                    return Err(invalid("cannot drop default database"));
                }
                if !self
                    .strings("SELECT name FROM collections WHERE db=?1", [db])?
                    .is_empty()
                {
                    return Err(Error::Conflict("database is not empty".into()));
                }
                self.conn
                    .execute("DELETE FROM databases WHERE name=?1", [db])?;
                return Ok(json!({}));
            }
            "databases/describe" => {
                self.database(db)?;
                return Ok(json!({"dbName":db}));
            }
            "collections/list" => {
                self.database(db)?;
                return Ok(json!(self.strings(
                    "SELECT name FROM collections WHERE db=?1 ORDER BY name",
                    [db]
                )?));
            }
            "collections/create" => return self.create_collection(db, r),
            "aliases/drop" => {
                self.database(db)?;
                if self.conn.execute(
                    "DELETE FROM aliases WHERE db=?1 AND name=?2",
                    params![db, text(r, "aliasName")?],
                )? == 0
                {
                    return Err(Error::NotFound("alias not found".into()));
                }
                return Ok(json!({}));
            }
            "aliases/describe" => {
                let alias = text(r, "aliasName")?;
                let c=self.conn.query_row("SELECT c.name FROM aliases a JOIN collections c ON c.id=a.cid WHERE a.db=?1 AND a.name=?2",params![db,alias],|r|r.get::<_,String>(0)).optional()?.ok_or_else(||Error::NotFound("alias not found".into()))?;
                return Ok(json!({"aliasName":alias,"collectionName":c,"dbName":db}));
            }
            "collections/has" => {
                self.database(db)?;
                return match self.collection(db, text(r, "collectionName")?) {
                    Ok(_) => Ok(json!({"has":true})),
                    Err(Error::NotFound(_)) => Ok(json!({"has":false})),
                    Err(e) => Err(e),
                };
            }
            _ => {}
        }
        let mut c = self.collection(db, text(r, "collectionName")?)?;
        match op {
            "collections/describe" => Ok(
                json!({"collectionName":c.name,"schema":c.schema,"indexes":c.indexes,"loadState":if c.loaded{"LoadStateLoaded"}else{"LoadStateNotLoad"}}),
            ),
            "collections/get_load_state" => Ok(
                json!({"loadState":if c.loaded{"LoadStateLoaded"}else{"LoadStateNotLoad"},"loadProgress":if c.loaded{100}else{0}}),
            ),
            "collections/get_stats" => Ok(
                json!({"rowCount":self.conn.query_row("SELECT COUNT(*) FROM entities WHERE cid=?1",[c.id],|r|r.get::<_,i64>(0))?}),
            ),
            "collections/drop" => {
                self.conn
                    .execute("DELETE FROM collections WHERE id=?1", [c.id])?;
                self.indexes.retain(|(id, _), _| *id != c.id);
                Ok(json!({}))
            }
            "collections/truncate" => {
                self.conn
                    .execute("DELETE FROM entities WHERE cid=?1", [c.id])?;
                self.indexes.retain(|(id, _), _| *id != c.id);
                Ok(json!({}))
            }
            "collections/rename" => {
                let n = text(r, "newCollectionName")?;
                self.available(db, n)?;
                self.conn.execute(
                    "UPDATE collections SET name=?1 WHERE id=?2",
                    params![n, c.id],
                )?;
                Ok(json!({}))
            }
            "collections/load" | "collections/release" => {
                let loaded = op.ends_with("/load");
                self.conn.execute(
                    "UPDATE collections SET loaded=?1 WHERE id=?2",
                    params![loaded, c.id],
                )?;
                self.conn.execute(
                    "UPDATE partitions SET loaded=?1 WHERE cid=?2",
                    params![loaded, c.id],
                )?;
                if !loaded {
                    self.indexes.retain(|(id, _), _| *id != c.id);
                }
                Ok(json!({}))
            }
            "partitions/list" => Ok(json!(self.strings(
                "SELECT name FROM partitions WHERE cid=?1 ORDER BY name",
                [c.id]
            )?)),
            "partitions/create" => {
                let p = text(r, "partitionName")?;
                name(p)?;
                if self.conn.execute(
                    "INSERT OR IGNORE INTO partitions(cid,name) VALUES(?1,?2)",
                    params![c.id, p],
                )? == 0
                {
                    return Err(Error::Conflict("partition already exists".into()));
                }
                Ok(json!({}))
            }
            "partitions/has" => match self.partition(&c, text(r, "partitionName")?) {
                Ok(()) => Ok(json!({"has":true})),
                Err(Error::NotFound(_)) => Ok(json!({"has":false})),
                Err(e) => Err(e),
            },
            "partitions/get_stats" => {
                let p = text(r, "partitionName")?;
                self.partition(&c, p)?;
                Ok(
                    json!({"rowCount":self.conn.query_row("SELECT COUNT(*) FROM entities WHERE cid=?1 AND partition=?2",params![c.id,p],|r|r.get::<_,i64>(0))?}),
                )
            }
            "partitions/drop" | "partitions/load" | "partitions/release" => {
                let ps = strings(r, "partitionNames")?;
                if ps.is_empty() {
                    return Err(invalid("partitionNames cannot be empty"));
                }
                for p in ps {
                    self.partition(&c, &p)?;
                    if op.ends_with("/drop") {
                        if p == "_default" {
                            return Err(invalid("cannot drop _default partition"));
                        }
                        self.conn.execute(
                            "DELETE FROM partitions WHERE cid=?1 AND name=?2",
                            params![c.id, p],
                        )?;
                    } else {
                        self.conn.execute(
                            "UPDATE partitions SET loaded=?1 WHERE cid=?2 AND name=?3",
                            params![op.ends_with("/load"), c.id, p],
                        )?;
                    }
                }
                self.indexes.retain(|(id, _), _| *id != c.id);
                Ok(json!({}))
            }
            "aliases/list" => Ok(json!(self.strings(
                "SELECT name FROM aliases WHERE cid=?1 ORDER BY name",
                [c.id]
            )?)),
            "aliases/create" | "aliases/alter" => {
                let a = text(r, "aliasName")?;
                name(a)?;
                if op.ends_with("/create") {
                    self.available(db, a)?;
                    self.conn
                        .execute("INSERT INTO aliases VALUES(?1,?2,?3)", params![db, a, c.id])?;
                } else if self.conn.execute(
                    "UPDATE aliases SET cid=?1 WHERE db=?2 AND name=?3",
                    params![c.id, db, a],
                )? == 0
                {
                    return Err(Error::NotFound("alias not found".into()));
                }
                Ok(json!({}))
            }
            "indexes/list" => Ok(json!(
                c.indexes.iter().map(|i| &i.index_name).collect::<Vec<_>>()
            )),
            "indexes/describe" => {
                let n = text(r, "indexName")?;
                let i = c
                    .indexes
                    .iter()
                    .find(|i| i.index_name == n)
                    .ok_or_else(|| Error::NotFound("index not found".into()))?;
                Ok(json!([i]))
            }
            "indexes/create" => {
                self.add_indexes(&mut c, &r["indexParams"])?;
                self.save_indexes(&c)?;
                self.indexes.retain(|(id, _), _| *id != c.id);
                Ok(json!({}))
            }
            "indexes/drop" => {
                let n = text(r, "indexName")?;
                let before = c.indexes.len();
                c.indexes.retain(|i| i.index_name != n);
                if before == c.indexes.len() {
                    return Err(Error::NotFound("index not found".into()));
                }
                self.save_indexes(&c)?;
                self.indexes.retain(|(id, _), _| *id != c.id);
                Ok(json!({}))
            }
            "entities/insert" | "entities/upsert" => self.write(&mut c, r, op.ends_with("/upsert")),
            "entities/query" | "entities/get" => self.query(&c, r, op.ends_with("/get")),
            "entities/delete" => self.delete(&c, r),
            "entities/search" => self.search(&c, r),
            "entities/hybrid_search" => self.hybrid(&c, r),
            _ => Err(Error::Unsupported(format!("operation {op}"))),
        }
    }
    fn create_collection(&mut self, db: &str, r: &Value) -> Result<Value> {
        self.database(db)?;
        let n = text(r, "collectionName")?;
        self.available(db, n)?;
        let schema = Schema::from_request(r)?;
        self.conn.execute(
            "INSERT INTO collections(db,name,schema_json) VALUES(?1,?2,?3)",
            params![db, n, serde_json::to_string(&schema)?],
        )?;
        let mut c = self.collection(db, n)?;
        self.conn.execute(
            "INSERT INTO partitions(cid,name) VALUES(?1,'_default')",
            [c.id],
        )?;
        if r.get("indexParams").is_some() {
            self.add_indexes(&mut c, &r["indexParams"])?;
        } else {
            for f in &c.schema.fields {
                if f.is_vector() {
                    let default_metric = match f.data_type.as_str() {
                        "SparseFloatVector" => "IP",
                        "BinaryVector" => "HAMMING",
                        _ => "COSINE",
                    };
                    let metric = r["metricType"].as_str().unwrap_or(default_metric);
                    let mut i: Index = serde_json::from_value(
                        json!({"fieldName":f.field_name,"metricType":metric}),
                    )?;
                    i.validate(f)?;
                    c.indexes.push(i);
                }
            }
        }
        self.save_indexes(&c)?;
        Ok(json!({}))
    }
    fn add_indexes(&self, c: &mut Collection, v: &Value) -> Result<()> {
        let indexes: Vec<Index> =
            serde_json::from_value(v.clone()).map_err(|e| invalid(e.to_string()))?;
        if indexes.is_empty() {
            return Err(invalid("indexParams cannot be empty"));
        }
        for mut i in indexes {
            i.validate(c.schema.field(&i.field_name)?)?;
            if c.indexes
                .iter()
                .any(|old| old.field_name == i.field_name || old.index_name == i.index_name)
            {
                return Err(Error::Conflict(
                    "index exists; drop it before replacing".into(),
                ));
            }
            c.indexes.push(i);
        }
        Ok(())
    }
    fn write(&mut self, c: &mut Collection, r: &Value, upsert: bool) -> Result<Value> {
        let data = r["data"]
            .as_array()
            .ok_or_else(|| invalid("data must be an array"))?;
        if data.is_empty() || data.len() > 10000 {
            return Err(invalid("write batch must have 1..10000 entities"));
        }
        let p = r["partitionName"].as_str().unwrap_or("_default");
        self.partition(c, p)?;
        let pk = c.schema.primary().field_name.clone();
        let mut ids = Vec::new();
        let mut seen = BTreeSet::new();
        for input in data {
            let mut row = input.clone();
            if !row.is_object() {
                return Err(invalid("entity must be an object"));
            }
            if c.schema.auto_id {
                if upsert {
                    if row.get(&pk).is_none() {
                        return Err(invalid("upsert requires primary key even with autoID"));
                    }
                } else {
                    if row.get(&pk).is_some() {
                        return Err(invalid("do not supply primary key with autoID insert"));
                    }
                    row[&pk] = json!(c.next_id);
                    c.next_id = c
                        .next_id
                        .checked_add(1)
                        .ok_or_else(|| invalid("autoID exhausted"))?;
                }
            }
            c.schema.validate_row(&mut row)?;
            let id = row[&pk].clone();
            let key = serde_json::to_string(&id)?;
            if !seen.insert(key.clone()) {
                return Err(invalid("duplicate primary key within batch"));
            }
            let exists: bool = self.conn.query_row(
                "SELECT EXISTS(SELECT 1 FROM entities WHERE cid=?1 AND pk=?2)",
                params![c.id, key],
                |r| r.get(0),
            )?;
            if exists && !upsert {
                return Err(Error::Conflict("primary key already exists".into()));
            }
            if c.schema.auto_id && upsert {
                let n = id.as_i64().unwrap();
                if n >= c.next_id {
                    c.next_id = n
                        .checked_add(1)
                        .ok_or_else(|| invalid("autoID exhausted"))?;
                }
            }
            self.conn.execute("INSERT INTO entities(cid,pk,partition,body) VALUES(?1,?2,?3,?4) ON CONFLICT(cid,pk) DO UPDATE SET partition=excluded.partition,body=excluded.body",params![c.id,key,p,serde_json::to_string(&row)?])?;
            ids.push(id);
        }
        self.conn.execute(
            "UPDATE collections SET next_id=?1 WHERE id=?2",
            params![c.next_id, c.id],
        )?;
        self.indexes.retain(|(id, _), _| *id != c.id);
        Ok(if upsert {
            json!({"upsertCount":ids.len(),"upsertIds":ids})
        } else {
            json!({"insertCount":ids.len(),"insertIds":ids})
        })
    }
    fn filter(&self, c: &Collection, r: &Value) -> Result<Expr> {
        let s = match r.get("filter") {
            None => "",
            Some(v) => v
                .as_str()
                .ok_or_else(|| invalid("filter must be a string"))?,
        };
        let expr = Expr::parse(s)?;
        let mut fields = Vec::new();
        expr.fields(&mut fields);
        if !c.schema.enable_dynamic_field {
            for f in fields {
                c.schema.field(f.split('.').next().unwrap())?;
            }
        }
        Ok(expr)
    }
    fn query(&self, c: &Collection, r: &Value, get: bool) -> Result<Value> {
        let expr = self.filter(c, r)?;
        let fields = output_fields(c, r)?;
        let mut rows = self.selected_rows(c, r, true)?;
        let ids = if get {
            let v = r.get("id").ok_or_else(|| invalid("id is required"))?;
            let ids = if let Some(a) = v.as_array() {
                a.clone()
            } else {
                vec![v.clone()]
            };
            for id in &ids {
                c.schema.primary().validate(id)?;
            }
            Some(ids)
        } else {
            None
        };
        rows.retain(|row| {
            expr.matches(row)
                && ids
                    .as_ref()
                    .is_none_or(|ids| ids.contains(&row[&c.schema.primary().field_name]))
        });
        if fields == ["count(*)"] {
            return Ok(json!([{"count(*)":rows.len()}]));
        }
        let order = strings(r, "orderByFields")?;
        for field in &order {
            let n = field.split(':').next().unwrap();
            let f = c.schema.field(n)?;
            if f.is_vector() || ["JSON", "Array"].contains(&f.data_type.as_str()) {
                return Err(invalid("ordering requires scalar fields"));
            }
            if field
                .split_once(':')
                .is_some_and(|(_, direction)| !["asc", "desc"].contains(&direction))
            {
                return Err(invalid("order direction must be asc or desc"));
            }
        }
        if !order.is_empty() {
            rows.sort_by(|a, b| {
                for o in &order {
                    let n = o.split(':').next().unwrap();
                    let mut cmp = compare(&a[n], &b[n])
                        .unwrap_or_else(|| a[n].is_null().cmp(&b[n].is_null()));
                    if o.ends_with(":desc") {
                        cmp = cmp.reverse();
                    }
                    if !cmp.is_eq() {
                        return cmp;
                    }
                }
                std::cmp::Ordering::Equal
            });
        }
        let (limit, offset) = pagination(r)?;
        Ok(json!(
            rows.iter()
                .skip(offset)
                .take(limit)
                .map(|row| project(row, &fields, &c.schema))
                .collect::<Vec<_>>()
        ))
    }
    fn delete(&mut self, c: &Collection, r: &Value) -> Result<Value> {
        if r["filter"].as_str().is_none_or(|s| s.trim().is_empty()) {
            return Err(invalid("delete requires a nonempty filter"));
        }
        let expr = self.filter(c, r)?;
        let rows = self.selected_rows(c, r, false)?;
        let mut count = 0;
        for row in rows {
            if expr.matches(&row) {
                count += self.conn.execute(
                    "DELETE FROM entities WHERE cid=?1 AND pk=?2",
                    params![
                        c.id,
                        serde_json::to_string(&row[&c.schema.primary().field_name])?
                    ],
                )?;
            }
        }
        self.indexes.retain(|(id, _), _| *id != c.id);
        Ok(json!({"deleteCount":count}))
    }
    fn search(&mut self, c: &Collection, r: &Value) -> Result<Value> {
        let expr = self.filter(c, r)?;
        let fields = output_fields(c, r)?;
        if fields.contains(&"count(*)".into()) {
            return Err(invalid("count(*) is only supported by query"));
        }
        let vectors: Vec<_> = c.schema.fields.iter().filter(|f| f.is_vector()).collect();
        let field = match r["annsField"].as_str() {
            Some(n) => c.schema.field(n)?,
            None if vectors.len() == 1 => vectors[0],
            _ => {
                return Err(invalid(
                    "annsField is required for multi-vector collections",
                ));
            }
        };
        let index = c
            .indexes
            .iter()
            .find(|i| i.field_name == field.field_name)
            .ok_or_else(|| Error::Conflict("vector field has no index".into()))?
            .clone();
        let data = r["data"]
            .as_array()
            .ok_or_else(|| invalid("search data must be an array of vectors"))?;
        if data.is_empty() || data.len() > 128 {
            return Err(invalid("search requires 1..128 query vectors"));
        }
        for q in data {
            field.validate(q)?;
        }
        let sp = &r["searchParams"];
        if !sp.is_null() {
            keys(sp, &["metricType", "params"], "searchParams")?;
        }
        if let Some(metric) = sp.get("metricType")
            && metric.as_str() != Some(&index.metric_type)
        {
            return Err(invalid("search metric must match index metric"));
        }
        let params = &sp["params"];
        if !params.is_null() {
            keys(
                params,
                &["nprobe", "ef", "radius", "range_filter"],
                "search params",
            )?;
        }
        let nprobe = number(&params["nprobe"], 8, 4096, "nprobe")?;
        if nprobe == 0 {
            return Err(invalid("nprobe must be positive"));
        }
        if index.index_type != "IVF_FLAT" && params.get("nprobe").is_some() {
            return Err(invalid("nprobe only applies to IVF_FLAT"));
        }
        let ef = number(&params["ef"], 128, 65536, "ef")?;
        if ef == 0 || (params.get("ef").is_some() && index.index_type != "HNSW") {
            return Err(invalid("ef must be positive and applies only to HNSW"));
        }
        let radius = optional_f64(params, "radius")?;
        let range = optional_f64(params, "range_filter")?;
        if range.is_some() && radius.is_none() {
            return Err(invalid("range_filter requires radius"));
        }
        if let (Some(lo), Some(hi)) = (range, radius)
            && ((ascending(&index.metric_type) && lo >= hi)
                || (!ascending(&index.metric_type) && lo <= hi))
        {
            return Err(invalid("invalid range bounds for metric"));
        }
        let group = match r.get("groupingField") {
            Some(v) => Some(
                v.as_str()
                    .ok_or_else(|| invalid("groupingField must be a string"))?,
            ),
            None => None,
        };
        if let Some(g) = group {
            let f = c.schema.field(g);
            if f.is_err() && !c.schema.enable_dynamic_field {
                return Err(invalid("unknown grouping field"));
            }
            if f.is_ok_and(|f| f.is_vector() || ["JSON", "Array"].contains(&f.data_type.as_str())) {
                return Err(invalid("grouping requires scalar field"));
            }
        }
        let (limit, offset) = pagination(r)?;
        let selected = self.selected_rows(c, r, true)?;
        if let Some(g) = group
            && selected
                .iter()
                .any(|row| row[g].is_array() || row[g].is_object())
        {
            return Err(invalid("grouping requires scalar field"));
        }
        let allowed: BTreeSet<_> = selected
            .iter()
            .filter(|row| expr.matches(row))
            .map(|row| row[&c.schema.primary().field_name].to_string())
            .collect();
        let rows = self.all_rows(c)?;
        let cache_key = (c.id, field.field_name.clone());
        if index.index_type != "FLAT" && !self.indexes.contains_key(&cache_key) {
            self.indexes
                .insert(cache_key.clone(), BuiltIndex::build(&rows, &index));
        }
        let mut out = Vec::new();
        for (qi, q) in data.iter().enumerate() {
            let candidates = if index.index_type != "FLAT" {
                self.indexes[&cache_key].candidates(q, nprobe, ef.max(limit + offset))
            } else {
                (0..rows.len()).collect()
            };
            let mut hits = Vec::new();
            for i in candidates {
                let row = &rows[i];
                if !allowed.contains(&row[&c.schema.primary().field_name].to_string()) {
                    continue;
                }
                let d = score(q, &row[&field.field_name], &index.metric_type);
                let asc = ascending(&index.metric_type);
                if radius.is_some_and(|v| if asc { d >= v } else { d <= v })
                    || range.is_some_and(|v| if asc { d < v } else { d > v })
                {
                    continue;
                }
                hits.push((i, d));
            }
            hits.sort_by(|a, b| {
                let cmp = if ascending(&index.metric_type) {
                    a.1.total_cmp(&b.1)
                } else {
                    b.1.total_cmp(&a.1)
                };
                cmp.then_with(|| a.0.cmp(&b.0))
            });
            let mut groups = BTreeSet::new();
            if let Some(g) = group {
                hits.retain(|(i, _)| groups.insert(rows[*i][g].to_string()));
            }
            for (i, d) in hits.into_iter().skip(offset).take(limit) {
                let row = &rows[i];
                let mut entity = project(row, &fields, &c.schema);
                entity["id"] = row[&c.schema.primary().field_name].clone();
                entity["distance"] = json!(d);
                if data.len() > 1 {
                    entity["queryIndex"] = json!(qi);
                }
                out.push(entity);
            }
        }
        Ok(json!(out))
    }
    fn hybrid(&mut self, c: &Collection, r: &Value) -> Result<Value> {
        let searches = r["search"]
            .as_array()
            .ok_or_else(|| invalid("search must be an array"))?;
        if searches.is_empty() || searches.len() > 16 {
            return Err(invalid("hybrid search requires 1..16 subsearches"));
        }
        keys(&r["rerank"], &["strategy", "params"], "rerank")?;
        let strategy = text(&r["rerank"], "strategy")?;
        if !["rrf", "weighted"].contains(&strategy) {
            return Err(Error::Unsupported(
                "reranker must be rrf or weighted".into(),
            ));
        }
        let rp = &r["rerank"]["params"];
        if !rp.is_null() {
            keys(
                rp,
                if strategy == "rrf" {
                    &["k"]
                } else {
                    &["weights"]
                },
                "rerank params",
            )?;
        }
        let k = optional_f64(rp, "k")?.unwrap_or(60.0);
        if k <= 0.0 {
            return Err(invalid("RRF k must be positive"));
        }
        let weights = if strategy == "weighted" {
            let a = rp["weights"]
                .as_array()
                .ok_or_else(|| invalid("weights required"))?;
            if a.len() != searches.len() {
                return Err(invalid("one weight per subsearch required"));
            }
            let mut ws = Vec::new();
            for v in a {
                let w = v
                    .as_f64()
                    .filter(|n| n.is_finite() && *n >= 0.0 && *n <= 1.0)
                    .ok_or_else(|| invalid("weights must be in [0,1]"))?;
                ws.push(w);
            }
            if ws.iter().sum::<f64>() == 0.0 {
                return Err(invalid("at least one weight must be positive"));
            }
            ws
        } else {
            vec![1.0; searches.len()]
        };
        let nq = searches[0]["data"].as_array().map(Vec::len).unwrap_or(0);
        let mut combined: BTreeMap<(usize, String), (Value, f64)> = BTreeMap::new();
        for (si, search) in searches.iter().enumerate() {
            keys(
                search,
                &[
                    "data",
                    "annsField",
                    "filter",
                    "limit",
                    "offset",
                    "metricType",
                    "params",
                ],
                "subsearch",
            )?;
            if search["data"].as_array().map(Vec::len) != Some(nq) {
                return Err(invalid("subsearches must have the same query count"));
            }
            let mut req = search.clone();
            req.as_object_mut().unwrap().remove("metricType");
            req.as_object_mut().unwrap().remove("params");
            req["outputFields"] = json!(["*"]);
            if let Some(p) = r.get("partitionNames") {
                req["partitionNames"] = p.clone();
            }
            let mut sp = json!({});
            if let Some(m) = search.get("metricType") {
                sp["metricType"] = m.clone();
            }
            if let Some(p) = search.get("params") {
                sp["params"] = p.clone();
            }
            req["searchParams"] = sp;
            let field = search["annsField"]
                .as_str()
                .map(str::to_owned)
                .or_else(|| {
                    c.schema
                        .fields
                        .iter()
                        .find(|f| f.is_vector())
                        .map(|f| f.field_name.clone())
                })
                .unwrap();
            let metric = c
                .indexes
                .iter()
                .find(|i| i.field_name == field)
                .map(|i| i.metric_type.clone())
                .ok_or_else(|| invalid("no index for subsearch"))?;
            let result = self.search(c, &req)?;
            let mut ranks = vec![0; nq];
            for hit in result.as_array().unwrap() {
                let qi = hit["queryIndex"].as_u64().unwrap_or(0) as usize;
                ranks[qi] += 1;
                let d = hit["distance"].as_f64().unwrap();
                let contribution = if strategy == "rrf" {
                    1.0 / (k + ranks[qi] as f64)
                } else {
                    let normalized = match metric.as_str() {
                        "COSINE" => (1.0 + d) / 2.0,
                        "IP" => 0.5 + d.atan() / std::f64::consts::PI,
                        _ => 1.0 - 2.0 * d.atan() / std::f64::consts::PI,
                    };
                    weights[si] * normalized
                };
                let key = (qi, hit["id"].to_string());
                let e = combined.entry(key).or_insert((hit.clone(), 0.0));
                e.1 += contribution;
            }
        }
        let fields = output_fields(c, r)?;
        let (limit, offset) = pagination(r)?;
        let mut out = Vec::new();
        for qi in 0..nq {
            let mut hits: Vec<_> = combined.iter().filter(|((q, _), _)| *q == qi).collect();
            hits.sort_by(|a, b| b.1.1.total_cmp(&a.1.1).then_with(|| a.0.1.cmp(&b.0.1)));
            for (_, (hit, d)) in hits.into_iter().skip(offset).take(limit) {
                let mut entity = project(hit, &fields, &c.schema);
                entity["id"] = hit["id"].clone();
                entity["distance"] = json!(d);
                if nq > 1 {
                    entity["queryIndex"] = json!(qi);
                } else {
                    entity.as_object_mut().unwrap().remove("queryIndex");
                }
                out.push(entity);
            }
        }
        Ok(json!(out))
    }
}
fn optional_f64(v: &Value, k: &str) -> Result<Option<f64>> {
    match v.get(k) {
        None => Ok(None),
        Some(x) => {
            Ok(Some(x.as_f64().filter(|n| n.is_finite()).ok_or_else(
                || invalid(format!("{k} must be finite number")),
            )?))
        }
    }
}
fn text<'a>(r: &'a Value, k: &str) -> Result<&'a str> {
    r[k].as_str()
        .filter(|s| !s.is_empty())
        .ok_or_else(|| invalid(format!("{k} must be a nonempty string")))
}
fn db_name(r: &Value) -> Result<&str> {
    match r.get("dbName") {
        None => Ok("default"),
        Some(_) => text(r, "dbName"),
    }
}
fn strings(r: &Value, k: &str) -> Result<Vec<String>> {
    match r.get(k) {
        None => Ok(vec![]),
        Some(v) => v
            .as_array()
            .ok_or_else(|| invalid(format!("{k} must be array")))?
            .iter()
            .map(|v| {
                v.as_str()
                    .map(str::to_owned)
                    .ok_or_else(|| invalid(format!("{k} elements must be strings")))
            })
            .collect(),
    }
}
fn pagination(r: &Value) -> Result<(usize, usize)> {
    let limit = number(&r["limit"], 10, 16384, "limit")?;
    let offset = number(&r["offset"], 0, 1000000, "offset")?;
    if limit == 0 {
        return Err(invalid("limit must be positive"));
    }
    Ok((limit, offset))
}
fn output_fields(c: &Collection, r: &Value) -> Result<Vec<String>> {
    let fields = strings(r, "outputFields")?;
    if fields.contains(&"count(*)".into()) && fields.len() != 1 {
        return Err(invalid("count(*) must be the only output field"));
    }
    for f in &fields {
        if f != "*" && f != "count(*)" && !c.schema.enable_dynamic_field {
            c.schema.field(f.split('.').next().unwrap())?;
        }
    }
    Ok(fields)
}
fn project(row: &Value, fields: &[String], s: &Schema) -> Value {
    if fields.iter().any(|f| f == "*") {
        return row.clone();
    }
    let mut out = json!({});
    let pk = &s.primary().field_name;
    out[pk] = row[pk].clone();
    for f in fields {
        out[f] = lookup(row, f).clone();
    }
    out
}
fn keys(v: &Value, allowed: &[&str], context: &str) -> Result<()> {
    let o = v
        .as_object()
        .ok_or_else(|| invalid(format!("{context} must be an object")))?;
    for k in o.keys() {
        if !allowed.contains(&k.as_str()) {
            return Err(Error::Unsupported(format!("{context} option {k}")));
        }
    }
    Ok(())
}
fn validate_request(op: &str, r: &Value) -> Result<()> {
    if !r.is_object() {
        return Err(invalid("request must be an object"));
    }
    for key in [
        "dbName",
        "collectionName",
        "newCollectionName",
        "aliasName",
        "partitionName",
        "annsField",
        "groupingField",
        "primaryFieldName",
        "vectorFieldName",
        "idType",
        "vectorFieldType",
        "metricType",
        "indexName",
    ] {
        if let Some(value) = r.get(key)
            && value.as_str().is_none_or(str::is_empty)
        {
            return Err(invalid(format!("{key} must be a nonempty string")));
        }
    }
    if let Some(value) = r.get("autoID")
        && !value.is_boolean()
    {
        return Err(invalid("autoID must be boolean"));
    }
    if op == "collections/create"
        && r.get("schema").is_some()
        && [
            "dimension",
            "idType",
            "autoID",
            "metricType",
            "vectorFieldType",
            "primaryFieldName",
            "vectorFieldName",
        ]
        .iter()
        .any(|k| r.get(*k).is_some())
    {
        return Err(invalid(
            "explicit schema cannot be combined with quick-create fields",
        ));
    }
    let mut allowed = vec!["dbName"];
    let extra: &[&str] = match op {
        "databases/list" | "databases/create" | "databases/drop" | "databases/describe"
        | "collections/list" => &[],
        "collections/create" => &[
            "collectionName",
            "dimension",
            "idType",
            "autoID",
            "metricType",
            "vectorFieldType",
            "primaryFieldName",
            "vectorFieldName",
            "schema",
            "indexParams",
        ],
        "collections/rename" => &["collectionName", "newCollectionName"],
        "collections/has"
        | "collections/describe"
        | "collections/get_stats"
        | "collections/drop"
        | "collections/truncate"
        | "collections/load"
        | "collections/release"
        | "collections/get_load_state"
        | "collections/flush"
        | "collections/compact"
        | "partitions/list"
        | "indexes/list"
        | "aliases/list" => &["collectionName"],
        "partitions/create" | "partitions/has" | "partitions/get_stats" => {
            &["collectionName", "partitionName"]
        }
        "partitions/drop" | "partitions/load" | "partitions/release" => {
            &["collectionName", "partitionNames"]
        }
        "aliases/create" | "aliases/alter" => &["collectionName", "aliasName"],
        "aliases/drop" | "aliases/describe" => &["aliasName"],
        "indexes/create" => &["collectionName", "indexParams"],
        "indexes/describe" | "indexes/drop" => &["collectionName", "indexName"],
        "entities/insert" | "entities/upsert" => &["collectionName", "partitionName", "data"],
        "entities/delete" => &["collectionName", "partitionNames", "filter"],
        "entities/query" => &[
            "collectionName",
            "partitionNames",
            "filter",
            "outputFields",
            "limit",
            "offset",
            "orderByFields",
        ],
        "entities/get" => &[
            "collectionName",
            "partitionNames",
            "id",
            "outputFields",
            "limit",
            "offset",
        ],
        "entities/search" => &[
            "collectionName",
            "partitionNames",
            "filter",
            "outputFields",
            "limit",
            "offset",
            "data",
            "annsField",
            "searchParams",
            "groupingField",
        ],
        "entities/hybrid_search" => &[
            "collectionName",
            "partitionNames",
            "outputFields",
            "limit",
            "offset",
            "search",
            "rerank",
        ],
        _ => return Err(Error::Unsupported(format!("operation {op}"))),
    };
    allowed.extend_from_slice(extra);
    if op.starts_with("entities/") {
        allowed.push("consistencyLevel");
        if let Some(v) = r.get("consistencyLevel")
            && v.as_str() != Some("Strong")
        {
            return Err(Error::Unsupported(
                "only Strong consistency is available".into(),
            ));
        }
    }
    keys(r, &allowed, "request")
}
