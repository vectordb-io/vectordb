use crate::{Error, Result, digest, identifier};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::BTreeMap;
use vectordb_engine::{
    segment::SegmentEngine,
    segment_storage::{SealedState, StateManifest},
};

pub const FORMAT: u32 = 2;
pub const IDEMPOTENCY_WINDOW_MS: i64 = 24 * 60 * 60 * 1000;
pub const MAX_DEDUP_BYTES: usize = 16 * 1024 * 1024;
pub fn now_ms() -> i64 {
    chrono::Utc::now().timestamp_millis()
}
/// Expiry is part of the key identity; changing it creates a new request.
pub fn idempotency_key(issued_ms: i64, nonce: &str) -> String {
    format!("v2-{issued_ms}-{nonce}")
}
pub fn expiry(id: &str, now: i64) -> Result<i64> {
    identifier(id)?;
    let mut parts = id.splitn(3, '-');
    if parts.next() != Some("v2") {
        return Err(Error::Invalid(
            "idempotency key must be v2-<issuedUnixMs>-<nonce>".into(),
        ));
    }
    let issued: i64 = parts
        .next()
        .and_then(|s| s.parse().ok())
        .filter(|n| *n >= 0)
        .ok_or_else(|| Error::Invalid("invalid idempotency timestamp".into()))?;
    if parts.next().is_none_or(str::is_empty) {
        return Err(Error::Invalid("idempotency nonce required".into()));
    }
    let expires = issued
        .checked_add(IDEMPOTENCY_WINDOW_MS)
        .ok_or_else(|| Error::Invalid("invalid idempotency timestamp".into()))?;
    if issued > now.saturating_add(60_000) {
        return Err(Error::Invalid(
            "idempotency timestamp is in the future".into(),
        ));
    }
    if now >= expires {
        return Err(Error::Conflict("idempotency key expired; do not retry as a new request without reconciling its outcome".into()));
    }
    Ok(expires)
}
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct Scope {
    pub cluster: String,
    pub collection: String,
    pub generation: String,
}
impl Scope {
    pub fn validate(&self) -> Result<()> {
        identifier(&self.cluster)?;
        identifier(&self.collection)?;
        identifier(&self.generation)
    }
    pub fn prefix(&self) -> String {
        format!("{}/{}/{}", self.cluster, self.collection, self.generation)
    }
    pub fn topic(&self) -> String {
        format!(
            "vdb.{}.{}.{}",
            self.cluster, self.collection, self.generation
        )
    }
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case", deny_unknown_fields)]
pub enum Entry {
    Activate {
        epoch: i64,
    },
    Barrier {
        epoch: i64,
    },
    Mutation {
        accepted_ms: i64,
        epoch: i64,
        request_id: String,
        operation: String,
        request: Value,
        result: Value,
        digest: String,
    },
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Record {
    pub format: u32,
    pub scope: Scope,
    pub entry: Entry,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Checkpoint {
    pub format: u32,
    pub scope: Scope,
    pub offset: i64,
    pub epoch: i64,
    pub clock_ms: i64,
    pub state: StateManifest,
    pub results: BTreeMap<String, (String, WriteResult)>,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct LogicalBackup {
    pub scope: Scope,
    pub spec: Option<vectordb_engine::segment::CollectionSpec>,
    pub rows: Vec<vectordb_engine::segment::VersionedRow>,
    pub results: BTreeMap<String, (String, WriteResult)>,
    pub clock_ms: i64,
}
pub struct Snapshot {
    pub checkpoint: Checkpoint,
    pub sealed: SealedState,
}
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct Token {
    pub scope: Scope,
    pub offset: i64,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WriteResult {
    pub data: Value,
    pub token: Token,
}
pub struct Replica {
    pub scope: Scope,
    pub offset: i64,
    pub epoch: i64,
    engine: SegmentEngine,
    admission_bytes: usize,
    clock_ms: i64,
    results: BTreeMap<String, (String, WriteResult)>,
    dedup_bytes: usize,
    expirations: BTreeMap<(i64, String), usize>,
}
impl Replica {
    pub fn empty(scope: Scope) -> Result<Self> {
        scope.validate()?;
        Ok(Self {
            scope,
            offset: -1,
            epoch: 0,
            engine: SegmentEngine::new(),
            admission_bytes: vectordb_engine::segment::MAX_STATE_BYTES,
            clock_ms: 0,
            results: BTreeMap::new(),
            dedup_bytes: 0,
            expirations: BTreeMap::new(),
        })
    }
    pub fn restore(
        checkpoint: Checkpoint,
        scope: &Scope,
        get: impl FnMut(&str) -> vectordb_engine::Result<Vec<u8>>,
    ) -> Result<Self> {
        if checkpoint.format != FORMAT
            || &checkpoint.scope != scope
            || checkpoint.offset < 0
            || checkpoint.epoch <= 0
            || checkpoint.clock_ms < 0
            || checkpoint.state.revision > checkpoint.offset
        {
            return Err(Error::Integrity(
                "checkpoint scope, format or watermark mismatch".into(),
            ));
        }
        let mut replica = Self::empty(scope.clone())?;
        replica.engine = SegmentEngine::load(checkpoint.state, get)?;
        replica.offset = checkpoint.offset;
        replica.epoch = checkpoint.epoch;
        replica.clock_ms = checkpoint.clock_ms;
        for (id, (hash, result)) in &checkpoint.results {
            expiry(id, checkpoint.clock_ms)?;
            if hash.len() != 64
                || !hash.bytes().all(|b| b.is_ascii_hexdigit())
                || result.token.scope != *scope
                || result.token.offset < 0
                || result.token.offset > checkpoint.offset
            {
                return Err(Error::Integrity("invalid dedup result".into()));
            }
        }
        replica.results = checkpoint.results;
        for (id, value) in &replica.results {
            let bytes = id.len() + serde_json::to_vec(value)?.len() + 8;
            replica
                .expirations
                .insert((expiry(id, replica.clock_ms)?, id.clone()), bytes);
            replica.dedup_bytes += bytes;
        }
        if replica.dedup_bytes > MAX_DEDUP_BYTES {
            return Err(Error::Integrity("dedup quota exceeded".into()));
        }
        Ok(replica)
    }
    pub fn content_digest(&self) -> Result<String> {
        use sha2::{Digest, Sha256};
        let mut hash = Sha256::new();
        for item in std::iter::once(serde_json::to_vec(&self.spec())).chain(
            self.engine
                .live_rows()
                .map(|row| serde_json::to_vec(&(&row.key, &row.partition, &row.body))),
        ) {
            let bytes = item?;
            hash.update((bytes.len() as u64).to_be_bytes());
            hash.update(bytes);
        }
        Ok(format!("{:x}", hash.finalize()))
    }
    pub fn logical_backup(&self) -> LogicalBackup {
        LogicalBackup {
            scope: self.scope.clone(),
            spec: self.spec().cloned(),
            rows: self.engine.live_rows().cloned().collect(),
            results: self.results.clone(),
            clock_ms: self.clock_ms,
        }
    }
    pub fn bootstrap(
        mut backup: LogicalBackup,
        scope: Scope,
        offset: i64,
        epoch: i64,
    ) -> Result<(Snapshot, String)> {
        if backup.scope.collection != scope.collection
            || backup
                .spec
                .as_ref()
                .is_some_and(|s| s.name != scope.collection)
        {
            return Err(Error::Integrity("logical backup identity mismatch".into()));
        }
        let engine = SegmentEngine::from_backup(backup.spec, backup.rows, offset)?;
        let mut replica = Self::empty(scope.clone())?;
        replica.offset = offset;
        replica.epoch = epoch;
        replica.clock_ms = backup.clock_ms.max(now_ms());
        for (_, value) in backup.results.values_mut() {
            if value.token.scope != backup.scope {
                return Err(Error::Integrity("foreign backup result".into()));
            }
            value.token = Token {
                scope: scope.clone(),
                offset,
            };
        }
        replica.engine = engine;
        replica.results = backup.results;
        replica.build_indexes()?;
        Ok((replica.checkpoint()?, replica.content_digest()?))
    }
    pub fn checkpoint(&self) -> Result<Snapshot> {
        let sealed = self.engine.seal()?;
        let clock = self.clock_ms;
        let results = self
            .results
            .iter()
            .filter(|(id, _)| expiry(id, clock).is_ok())
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect();
        let checkpoint = Checkpoint {
            format: FORMAT,
            scope: self.scope.clone(),
            offset: self.offset,
            epoch: self.epoch,
            clock_ms: clock,
            state: sealed.manifest.clone(),
            results,
        };
        Ok(Snapshot { checkpoint, sealed })
    }
    pub fn spec(&self) -> Option<&vectordb_engine::segment::CollectionSpec> {
        self.engine.spec()
    }
    pub fn reclaim(&mut self) {
        self.engine.reclaim_tombstones();
    }
    pub fn result(&self, id: &str) -> Option<WriteResult> {
        self.results.get(id).map(|(_, result)| result.clone())
    }
    pub fn limit_writes(&mut self, bytes: usize) {
        self.admission_bytes = bytes;
    }
    pub fn prepare(
        &self,
        epoch: i64,
        id: &str,
        operation: &str,
        request: Value,
    ) -> Result<std::result::Result<Record, WriteResult>> {
        self.prepare_at(epoch, id, operation, request, now_ms())
    }
    pub fn prepare_at(
        &self,
        epoch: i64,
        id: &str,
        operation: &str,
        request: Value,
        now: i64,
    ) -> Result<std::result::Result<Record, WriteResult>> {
        let now = now.max(self.clock_ms);
        expiry(id, now)?;
        if epoch != self.epoch || epoch <= 0 {
            return Err(Error::Conflict("stale owner epoch".into()));
        }
        identifier(id)?;
        self.validate_request(operation, &request, true)?;
        let hash = request_digest(operation, &request)?;
        if let Some((old, result)) = self.results.get(id) {
            if old != &hash {
                return Err(Error::Conflict(
                    "idempotency key reused with different request".into(),
                ));
            }
            return Ok(Err(result.clone()));
        }
        let result = self
            .engine
            .prepare_with_limit(operation, request.clone(), self.admission_bytes)?
            .result;
        let retained = self.retained_bytes(now);
        if retained + id.len() + serde_json::to_vec(&result)?.len() + 1024 > MAX_DEDUP_BYTES {
            return Err(Error::Unavailable(
                "idempotency state quota exhausted; wait for window expiry".into(),
            ));
        }
        Ok(Ok(Record {
            format: FORMAT,
            scope: self.scope.clone(),
            entry: Entry::Mutation {
                accepted_ms: now,
                epoch,
                request_id: id.into(),
                operation: operation.into(),
                request,
                result,
                digest: hash,
            },
        }))
    }
    pub fn check_capacity(&self, record: &Record) -> Result<()> {
        if serde_json::to_vec(record)?.len() > 1024 * 1024 {
            return Err(Error::Invalid("WAL entry exceeds 1 MiB".into()));
        }
        Ok(())
    }
    pub fn apply(&mut self, offset: i64, record: Record) -> Result<()> {
        self.check_capacity(&record)?;
        if record.format != FORMAT || record.scope != self.scope || offset <= self.offset {
            return Err(Error::Integrity(
                "invalid record scope, version or offset order".into(),
            ));
        }
        match &record.entry {
            Entry::Activate { epoch } => {
                if *epoch <= self.epoch {
                    return Err(Error::Integrity("non-increasing owner epoch".into()));
                }
                self.epoch = *epoch;
            }
            Entry::Barrier { epoch } => self.check_epoch(*epoch)?,
            Entry::Mutation {
                accepted_ms,
                epoch,
                request_id,
                operation,
                request,
                result,
                digest: hash,
            } => {
                self.check_epoch(*epoch)?;
                if *accepted_ms < self.clock_ms {
                    return Err(Error::Integrity("WAL clock regressed".into()));
                }
                expiry(request_id, *accepted_ms)?;
                self.validate_request(operation, request, true)?;
                identifier(request_id)?;
                if &request_digest(operation, request)? != hash
                    || self.results.contains_key(request_id)
                {
                    return Err(Error::Integrity(
                        "duplicate or corrupt committed mutation".into(),
                    ));
                }
                // Publish state only after the persisted result has been verified.
                let batch = self.engine.prepare(operation, request.clone())?;
                if &batch.result != result {
                    return Err(Error::Integrity("non-deterministic replay result".into()));
                }
                let value = (
                    hash.clone(),
                    WriteResult {
                        data: result.clone(),
                        token: Token {
                            scope: self.scope.clone(),
                            offset,
                        },
                    },
                );
                let bytes = request_id.len() + serde_json::to_vec(&value)?.len() + 8;
                if self.retained_bytes(*accepted_ms) + bytes > MAX_DEDUP_BYTES {
                    return Err(Error::Integrity(
                        "committed dedup state exceeds quota".into(),
                    ));
                }
                self.engine.commit(batch, offset)?;
                self.clock_ms = *accepted_ms;
                while self
                    .expirations
                    .first_key_value()
                    .is_some_and(|((expires, _), _)| *expires <= *accepted_ms)
                {
                    let ((_, id), charge) = self.expirations.pop_first().unwrap();
                    self.results.remove(&id);
                    self.dedup_bytes -= charge;
                }
                self.expirations.insert(
                    (expiry(request_id, *accepted_ms)?, request_id.clone()),
                    bytes,
                );
                self.dedup_bytes += bytes;
                self.results.insert(request_id.clone(), value);
            }
        }
        self.offset = offset;

        Ok(())
    }
    fn retained_bytes(&self, now: i64) -> usize {
        self.dedup_bytes
            - self
                .expirations
                .iter()
                .take_while(|((expires, _), _)| *expires <= now)
                .map(|(_, bytes)| bytes)
                .sum::<usize>()
    }
    fn check_epoch(&self, epoch: i64) -> Result<()> {
        if epoch <= 0 || epoch != self.epoch {
            return Err(Error::Integrity("record from inactive epoch".into()));
        }
        Ok(())
    }
    pub fn read_view(
        &self,
        op: &str,
        request: &Value,
    ) -> Result<vectordb_engine::query_index::ReadView> {
        self.validate_request(op, request, false)?;
        Ok(self.engine.read_view()?)
    }
    pub fn state_bytes(&self) -> usize {
        self.engine.state_bytes()
    }
    pub fn build_indexes(&self) -> Result<()> {
        Ok(self.engine.build_indexes()?)
    }
    pub fn stop_index_jobs(&self) {
        self.engine.cancel_index_builds();
    }
    pub fn index_jobs(&self) -> Vec<vectordb_engine::query_index::IndexJob> {
        self.engine.index_jobs()
    }
    pub fn index_control(&self, action: &str, request: &Value) -> Result<Value> {
        self.validate_request("collections/describe", request, false)?;
        self.engine
            .execute("collections/describe", request.clone())?;
        match action {
            "build" | "retry" => self.engine.request_index_build(),
            "cancel" => self.engine.cancel_index_builds(),
            "status" => {}
            _ => return Err(Error::Invalid("unknown index control".into())),
        }
        Ok(self.engine.index_status())
    }
    pub fn query(&mut self, operation: &str, request: Value) -> Result<Value> {
        self.validate_request(operation, &request, false)?;
        Ok(self.engine.execute(operation, request)?)
    }
    fn validate_request(&self, operation: &str, request: &Value, write: bool) -> Result<()> {
        if request.get("dbName").is_some_and(|v| v != "default")
            || request["collectionName"] != self.scope.collection
        {
            return Err(Error::Invalid(
                "request must target this process's collection in default database".into(),
            ));
        }
        let allowed = if write {
            matches!(
                operation,
                "collections/create"
                    | "collections/drop"
                    | "partitions/drop"
                    | "entities/insert"
                    | "entities/upsert"
                    | "entities/delete"
                    | "indexes/create"
                    | "indexes/drop"
                    | "partitions/create"
            )
        } else {
            matches!(
                operation,
                "collections/describe"
                    | "collections/has"
                    | "collections/get_stats"
                    | "collections/get_load_state"
                    | "entities/search"
                    | "entities/hybrid_search"
                    | "entities/get"
                    | "entities/query"
                    | "entities/count"
                    | "indexes/list"
                    | "indexes/describe"
                    | "partitions/list"
                    | "partitions/has"
                    | "partitions/get_stats"
            )
        };
        if !allowed {
            return Err(Error::Unsupported(format!(
                "{operation} is not enabled in cluster mode"
            )));
        }
        Ok(())
    }
}
pub fn request_digest(operation: &str, request: &Value) -> Result<String> {
    Ok(digest(&serde_json::to_vec(&(operation, request))?))
}
