//! M0 boundaries. The concrete adapters implement these same contracts; no mock
//! backend semantics are substituted for the integration contract tests.
use crate::{
    Error, Result,
    state::{Record, Scope, Token},
};
use serde::{Deserialize, Serialize};
use serde_json::Value;
pub use vectordb_engine::segment::CollectionSpec;
pub use vectordb_engine::segment_storage::SegmentManifest;
pub type CommitToken = Token;
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct MutationBatch {
    pub format: u32,
    pub scope: Scope,
    pub owner_epoch: i64,
    pub request_id: String,
    pub operation: String,
    pub request: Value,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ReadSnapshot {
    pub format: u32,
    pub token: CommitToken,
    pub manifest_key: String,
    pub pin_key: String,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum TaskKind {
    Flush,
    BuildIndex,
    Compact,
    Load,
    Retire,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TaskSpec {
    pub format: u32,
    pub id: String,
    pub scope: Scope,
    pub kind: TaskKind,
    pub owner_epoch: i64,
    pub input_manifest: String,
    pub attempt: u32,
}

pub trait Catalog {
    fn latest(&self) -> Result<Option<crate::catalog::Manifest>>;
    fn publish(&self, manifest: &crate::catalog::Manifest) -> Result<bool>;
    fn pin_latest(&self) -> Result<Option<(crate::catalog::Manifest, String, String)>>;
    fn verify_pin(&self, key: &str) -> Result<()>;
}
pub trait DurableLog {
    fn append(&self, record: &Record) -> Result<i64>;
    fn replay(
        &self,
        after: i64,
        through: i64,
        apply: &mut dyn FnMut(i64, Record) -> Result<()>,
    ) -> Result<()>;
    fn trim(&self, through: i64) -> Result<()>;
}
pub trait ObjectStore {
    /// Must fail if the key already exists; overwriting is never allowed.
    fn put_new(&self, key: &str, bytes: Vec<u8>) -> Result<()>;
    fn get(&self, key: &str) -> Result<Vec<u8>>;
    /// Idempotent; only the catalog reclamation protocol may authorize deletion.
    fn delete(&self, key: &str) -> Result<()>;
}
pub trait SegmentReader {
    fn version(&self) -> i64;
    fn get(&self, primary_key: &Value) -> Option<vectordb_engine::segment::VersionedRow>;
}
pub trait IndexReader {
    fn search(&mut self, request: Value) -> vectordb_engine::Result<Value>;
}
impl Catalog for crate::catalog::Catalog {
    fn latest(&self) -> Result<Option<crate::catalog::Manifest>> {
        self.manifest()
    }
    fn publish(&self, m: &crate::catalog::Manifest) -> Result<bool> {
        self.publish(m)
    }
    fn pin_latest(&self) -> Result<Option<(crate::catalog::Manifest, String, String)>> {
        self.pin_latest()
    }
    fn verify_pin(&self, key: &str) -> Result<()> {
        self.verify_pin(key)
    }
}
impl DurableLog for crate::kafka::Log {
    fn append(&self, r: &Record) -> Result<i64> {
        self.append(r)
    }
    fn replay(&self, a: i64, b: i64, f: &mut dyn FnMut(i64, Record) -> Result<()>) -> Result<()> {
        self.replay(a, b, f)
    }
    fn trim(&self, b: i64) -> Result<()> {
        self.trim(b)
    }
}
impl ObjectStore for crate::object::Objects {
    fn put_new(&self, k: &str, b: Vec<u8>) -> Result<()> {
        self.put(k, b)
    }
    fn get(&self, k: &str) -> Result<Vec<u8>> {
        self.get(k)
    }
    fn delete(&self, k: &str) -> Result<()> {
        self.delete(k)
    }
}
impl SegmentReader for vectordb_engine::segment::SegmentEngine {
    fn version(&self) -> i64 {
        self.revision()
    }
    fn get(&self, k: &Value) -> Option<vectordb_engine::segment::VersionedRow> {
        self.get_version(k)
    }
}
impl IndexReader for vectordb_engine::segment::SegmentEngine {
    fn search(&mut self, r: Value) -> vectordb_engine::Result<Value> {
        self.execute("entities/search", r)
    }
}
/// Kept in sync with proto/vectordb/internal/v1/core.proto. Avoids requiring a
/// host protoc binary; wire fixtures cover tags and additive compatibility.
#[derive(Clone, PartialEq, prost::Message)]
pub struct Envelope {
    #[prost(uint32, tag = "1")]
    pub protocol_version: u32,
    #[prost(string, tag = "2")]
    pub collection_generation: String,
    #[prost(enumeration = "Kind", tag = "3")]
    pub kind: i32,
    #[prost(bytes = "vec", tag = "4")]
    pub body: Vec<u8>,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq, prost::Enumeration)]
#[repr(i32)]
pub enum Kind {
    Unspecified = 0,
    CollectionSpec = 1,
    MutationBatch = 2,
    CommitToken = 3,
    ReadSnapshot = 4,
    SegmentManifest = 5,
    TaskSpec = 6,
}
impl Envelope {
    pub fn validate(&self, expected_generation: &str) -> Result<()> {
        if self.protocol_version != 1
            || self.collection_generation != expected_generation
            || Kind::try_from(self.kind).map_or(true, |k| k == Kind::Unspecified)
        {
            return Err(Error::Unsupported(
                "internal protocol version, kind or generation".into(),
            ));
        }
        if self.body.len() > 1024 * 1024 {
            return Err(Error::Invalid("internal envelope exceeds 1 MiB".into()));
        }
        Ok(())
    }
}
