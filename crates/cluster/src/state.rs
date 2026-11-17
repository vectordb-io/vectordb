use crate::{Error, Result, digest, identifier};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::BTreeMap;
use vectordb_engine::Engine;

pub const FORMAT: u32 = 1;
// Bound the experimental replay checkpoint until compact segment snapshots exist.
pub const MAX_HISTORY_BYTES: usize = 48 * 1024 * 1024;
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
pub struct SavedRecord {
    pub offset: i64,
    pub record: Record,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Checkpoint {
    pub format: u32,
    pub scope: Scope,
    pub offset: i64,
    pub records: Vec<SavedRecord>,
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
    engine: Engine,
    records: Vec<SavedRecord>,
    results: BTreeMap<String, (String, WriteResult)>,
    history_bytes: usize,
}
impl Replica {
    pub fn empty(scope: Scope) -> Result<Self> {
        scope.validate()?;
        Ok(Self {
            scope,
            offset: -1,
            epoch: 0,
            engine: Engine::memory()?,
            records: vec![],
            results: BTreeMap::new(),
            history_bytes: 0,
        })
    }
    pub fn restore(checkpoint: Checkpoint, scope: &Scope) -> Result<Self> {
        if checkpoint.format != FORMAT || &checkpoint.scope != scope {
            return Err(Error::Integrity(
                "checkpoint scope or format mismatch".into(),
            ));
        }
        let mut replica = Self::empty(scope.clone())?;
        for saved in checkpoint.records {
            replica.apply(saved.offset, saved.record)?;
        }
        if replica.offset != checkpoint.offset {
            return Err(Error::Integrity(
                "checkpoint offset does not match its contents".into(),
            ));
        }
        Ok(replica)
    }
    pub fn checkpoint(&self) -> Checkpoint {
        Checkpoint {
            format: FORMAT,
            scope: self.scope.clone(),
            offset: self.offset,
            records: self.records.clone(),
        }
    }
    pub fn result(&self, id: &str) -> Option<WriteResult> {
        self.results.get(id).map(|(_, result)| result.clone())
    }
    pub fn prepare(
        &self,
        epoch: i64,
        id: &str,
        operation: &str,
        request: Value,
    ) -> Result<std::result::Result<Record, WriteResult>> {
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
        let mut preview = self.engine.fork_memory()?;
        let result = preview.execute(operation, request.clone())?;
        Ok(Ok(Record {
            format: FORMAT,
            scope: self.scope.clone(),
            entry: Entry::Mutation {
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
        if self.history_bytes + serde_json::to_vec(record)?.len() + 128 > MAX_HISTORY_BYTES {
            return Err(Error::Unavailable(
                "P1 replay history limit reached; migrate to a fresh generation".into(),
            ));
        }
        Ok(())
    }
    pub fn apply(&mut self, offset: i64, record: Record) -> Result<()> {
        self.check_capacity(&record)?;
        let record_bytes = serde_json::to_vec(&record)?.len() + 128;
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
                epoch,
                request_id,
                operation,
                request,
                result,
                digest: hash,
            } => {
                self.check_epoch(*epoch)?;
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
                let mut next = self.engine.fork_memory()?;
                let actual = next.execute(operation, request.clone())?;
                if &actual != result {
                    return Err(Error::Integrity("non-deterministic replay result".into()));
                }
                self.engine = next;
                self.results.insert(
                    request_id.clone(),
                    (
                        hash.clone(),
                        WriteResult {
                            data: result.clone(),
                            token: Token {
                                scope: self.scope.clone(),
                                offset,
                            },
                        },
                    ),
                );
            }
        }
        self.offset = offset;
        self.history_bytes += record_bytes;
        self.records.push(SavedRecord { offset, record });
        Ok(())
    }
    fn check_epoch(&self, epoch: i64) -> Result<()> {
        if epoch <= 0 || epoch != self.epoch {
            return Err(Error::Integrity("record from inactive epoch".into()));
        }
        Ok(())
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
                    | "entities/insert"
                    | "entities/upsert"
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
                    | "entities/get"
                    | "entities/query"
                    | "indexes/list"
                    | "indexes/describe"
                    | "partitions/list"
                    | "partitions/has"
                    | "partitions/get_stats"
            )
        };
        if !allowed {
            return Err(Error::Unsupported(format!(
                "{operation} is not enabled in P1 cluster mode"
            )));
        }
        Ok(())
    }
}
pub fn request_digest(operation: &str, request: &Value) -> Result<String> {
    Ok(digest(&serde_json::to_vec(&(operation, request))?))
}
