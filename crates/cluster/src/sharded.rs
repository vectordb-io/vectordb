//! Versioned query routing and a shared-WAL, assigned-shard-only replica.
use crate::{
    Error, Result,
    catalog::Manifest,
    state::{Checkpoint, Entry, FORMAT, Record, Scope, Token, request_digest},
};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::{BTreeMap, BTreeSet};
use vectordb_engine::{query_index::ReadView, segment::SegmentEngine};
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct QueryRoutes {
    pub epoch: u64,
    pub hash_version: u32,
    pub shards: Vec<Vec<String>>,
}
impl QueryRoutes {
    pub fn validate(&self, count: usize) -> Result<()> {
        vectordb_engine::shard::validate_count(count)?;
        if self.epoch == 0 || self.hash_version != 1 || self.shards.len() != count {
            return Err(Error::Invalid("invalid query routing layout".into()));
        }
        let mut nodes = BTreeSet::new();
        for replicas in &self.shards {
            let unique = replicas.iter().collect::<BTreeSet<_>>();
            if replicas.is_empty() || replicas.len() > 3 || unique.len() != replicas.len() {
                return Err(Error::Invalid(
                    "every shard requires 1..3 distinct replicas".into(),
                ));
            }
            for address in replicas {
                let url = reqwest::Url::parse(address)
                    .map_err(|_| Error::Invalid("invalid query address".into()))?;
                if !matches!(url.scheme(), "http" | "https")
                    || !url.username().is_empty()
                    || url.password().is_some()
                    || url.query().is_some()
                    || url.fragment().is_some()
                    || url.path() != "/"
                    || address.ends_with('/')
                {
                    return Err(Error::Invalid(
                        "query address must be an HTTP(S) origin without trailing slash".into(),
                    ));
                }
                nodes.insert(address);
            }
        }
        if nodes.len() > 32 {
            return Err(Error::Invalid("query node quota exceeded".into()));
        }
        Ok(())
    }
    pub fn assigned(&self, address: &str) -> BTreeSet<usize> {
        self.shards
            .iter()
            .enumerate()
            .filter(|(_, r)| r.iter().any(|r| r == address))
            .map(|(n, _)| n)
            .collect()
    }
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ShardQuery {
    pub token: Token,
    pub route_epoch: u64,
    pub shard_count: usize,
    pub shards: BTreeSet<usize>,
    pub manifest: Option<Manifest>,
    pub pin: Option<String>,
    pub operation: String,
    pub requests: Vec<Value>,
    pub deadline_ms: i64,
    #[serde(default)]
    pub warm_version: Option<u64>,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ShardPart {
    pub shard: usize,
    pub values: Vec<Value>,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ShardReply {
    pub token: Token,
    pub route_epoch: u64,
    pub parts: Vec<ShardPart>,
}
impl ShardReply {
    pub fn validate(&self, request: &ShardQuery) -> Result<()> {
        let ids = self.parts.iter().map(|p| p.shard).collect::<BTreeSet<_>>();
        if self.token != request.token
            || self.route_epoch != request.route_epoch
            || ids != request.shards
            || ids.len() != self.parts.len()
            || self
                .parts
                .iter()
                .any(|p| p.values.len() != request.requests.len())
        {
            return Err(Error::Integrity(
                "incomplete or mixed-snapshot shard response".into(),
            ));
        }
        Ok(())
    }
}
pub struct ShardReplica {
    pub scope: Scope,
    pub offset: i64,
    pub epoch: i64,
    pub count: usize,
    pub engines: BTreeMap<usize, SegmentEngine>,
    pub records: u64,
}
impl ShardReplica {
    pub fn empty(scope: Scope, count: usize, assigned: &BTreeSet<usize>) -> Result<Self> {
        scope.validate()?;
        let engines = assigned
            .iter()
            .map(|s| Ok((*s, SegmentEngine::empty_shard(count, *s)?)))
            .collect::<Result<BTreeMap<_, _>>>()?;
        if engines.is_empty() {
            return Err(Error::Invalid("empty shard assignment".into()));
        }
        Ok(Self {
            scope,
            offset: -1,
            epoch: 0,
            count,
            engines,
            records: 0,
        })
    }
    pub fn restore(
        checkpoint: Checkpoint,
        scope: &Scope,
        count: usize,
        assigned: &BTreeSet<usize>,
        get: impl FnMut(&str) -> vectordb_engine::Result<Vec<u8>>,
    ) -> Result<Self> {
        if checkpoint.format != FORMAT
            || checkpoint.scope != *scope
            || checkpoint.offset < 0
            || checkpoint.epoch <= 0
            || checkpoint.state.revision > checkpoint.offset
            || checkpoint
                .state
                .spec
                .as_ref()
                .is_none_or(|s| s.shard_count != count)
        {
            return Err(Error::Integrity(
                "invalid shard checkpoint identity/layout".into(),
            ));
        }
        let engine = SegmentEngine::load_shards(checkpoint.state, assigned.clone(), get)?;
        Ok(Self {
            scope: scope.clone(),
            offset: checkpoint.offset,
            epoch: checkpoint.epoch,
            count,
            engines: engine.split_shards(assigned)?.into_iter().collect(),
            records: 0,
        })
    }
    pub fn apply(&mut self, offset: i64, record: Record) -> Result<()> {
        if record.format != FORMAT || record.scope != self.scope || offset <= self.offset {
            return Err(Error::Integrity("invalid shard WAL identity/order".into()));
        }
        match record.entry {
            Entry::Activate { epoch } => {
                if epoch <= self.epoch {
                    return Err(Error::Integrity("non-increasing epoch".into()));
                }
                self.epoch = epoch;
            }
            Entry::Barrier { epoch } => self.check_epoch(epoch)?,
            Entry::Mutation {
                epoch,
                operation,
                request,
                result,
                digest,
                ..
            } => {
                self.check_epoch(epoch)?;
                if request["collectionName"] != self.scope.collection
                    || request_digest(&operation, &request)? != digest
                {
                    return Err(Error::Integrity("corrupt shard mutation".into()));
                }
                // Publish every local shard atomically. Persistent trees share row bodies.
                let mut next = self.engines.clone();
                for (shard, engine) in &mut next {
                    engine
                        .apply_shard(&operation, &request, &result, offset, self.count, *shard)?;
                }
                self.engines = next;
            }
        }
        self.offset = offset;
        self.records += 1;
        Ok(())
    }
    fn check_epoch(&self, epoch: i64) -> Result<()> {
        if epoch <= 0 || epoch != self.epoch {
            Err(Error::Integrity("inactive shard writer epoch".into()))
        } else {
            Ok(())
        }
    }
    pub fn views(&self, token: &Token, shards: &BTreeSet<usize>) -> Result<Vec<(usize, ReadView)>> {
        if token.scope != self.scope || token.offset != self.offset {
            return Err(Error::Conflict("shard snapshot is not exactly B".into()));
        }
        shards
            .iter()
            .map(|s| {
                let engine = self
                    .engines
                    .get(s)
                    .ok_or_else(|| Error::Conflict("shard not assigned".into()))?;
                Ok((*s, engine.read_view()?))
            })
            .collect()
    }
    pub fn rows(&self) -> usize {
        self.engines.values().map(SegmentEngine::row_count).sum()
    }
    pub fn bytes(&self) -> usize {
        self.engines.values().map(SegmentEngine::state_bytes).sum()
    }
}
