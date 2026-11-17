//! Hash v1: FNV-1a over canonical JSON primary-key bytes. JSON's quoted string
//! encoding is the type tag; integer 1 and string "1" are distinct keys.
use crate::{
    Error, Result,
    error::invalid,
    segment::{MutationBatch, SegmentEngine},
};
use serde_json::{Value, json};
use std::collections::BTreeSet;
pub const HASH_VERSION: u32 = 1;
pub fn one() -> usize {
    1
}
pub fn validate_count(count: usize) -> Result<()> {
    if !count.is_power_of_two() || count > 32 {
        return Err(invalid("shardCount must be one of 1,2,4,8,16,32"));
    }
    Ok(())
}
pub fn hash_v1(key: &str) -> u64 {
    key.bytes().fold(0xcbf29ce484222325u64, |h, b| {
        (h ^ u64::from(b)).wrapping_mul(0x100000001b3)
    })
}
pub fn shard_for(key: &str, count: usize) -> usize {
    (hash_v1(key) % (count as u64)) as usize
}
impl SegmentEngine {
    pub fn empty_shard(count: usize, shard: usize) -> Result<Self> {
        validate_count(count)?;
        if shard >= count {
            return Err(invalid("shard outside layout"));
        }
        let mut e = Self::with_query_limits(
            256 * 1024 * 1024 / count,
            2 * 1024 * 1024 * 1024 / count,
            128 * 1024 * 1024 / count,
            256 * 1024 * 1024 / count,
        );
        e.selection = Some(BTreeSet::from([shard]));
        Ok(e)
    }
    pub fn split_shards(&self, shards: &BTreeSet<usize>) -> Result<Vec<(usize, Self)>> {
        let count = self.spec.as_ref().map_or(1, |c| c.shard_count);
        validate_count(count)?;
        if shards.iter().any(|s| *s >= count) {
            return Err(invalid("invalid shard selection"));
        }
        let indexes = self.persist_indexes()?;
        let mut out = vec![];
        for &shard in shards {
            let mut e = Self::empty_shard(count, shard)?;
            e.spec = self.spec.clone();
            e.revision = self.revision;
            e.selection = Some(BTreeSet::from([shard]));
            e.rows = self
                .rows
                .iter()
                .filter(|(k, _)| shard_for(k, count) == shard)
                .map(|(k, r)| (k.clone(), r.clone()))
                .collect();
            for (key, bytes) in &indexes {
                let bucket = key
                    .strip_prefix("index-bucket-")
                    .and_then(|s| s.strip_suffix(".json"))
                    .and_then(|s| s.parse::<usize>().ok())
                    .unwrap();
                if bucket % count == shard {
                    let reference = crate::segment_storage::ObjectRef {
                        key: key.clone(),
                        bytes: bytes.len(),
                        sha256: crate::segment_storage::checksum(bytes),
                    };
                    e.load_index(&reference, bytes.clone())?;
                }
            }
            e.recount_bytes();
            out.push((shard, e));
        }
        Ok(out)
    }
    /// Apply a trusted committed batch without retaining entities from other shards.
    /// All shards advance the global auto-ID watermark, even on an empty local delta.
    pub fn apply_shard(
        &mut self,
        op: &str,
        request: &Value,
        result: &Value,
        version: i64,
        count: usize,
        shard: usize,
    ) -> Result<()> {
        if self.selection.as_ref() != Some(&BTreeSet::from([shard])) {
            return Err(invalid("engine is not assigned to this shard"));
        }
        validate_count(count)?;
        let batch = if matches!(op, "entities/insert" | "entities/upsert") {
            let c = self
                .spec
                .as_ref()
                .ok_or_else(|| invalid("mutation before collection create"))?;
            if c.shard_count != count {
                return Err(invalid("shard layout changed"));
            }
            let data = request["data"]
                .as_array()
                .ok_or_else(|| invalid("invalid committed data"))?;
            let key = if op == "entities/insert" {
                "insertIds"
            } else {
                "upsertIds"
            };
            let ids = result[key]
                .as_array()
                .ok_or_else(|| invalid("committed IDs missing"))?;
            let count_key = if op == "entities/insert" {
                "insertCount"
            } else {
                "upsertCount"
            };
            if ids.len() != data.len()
                || data.is_empty()
                || result[count_key].as_u64() != Some(data.len() as u64)
            {
                return Err(invalid("committed ID count mismatch"));
            }
            let pk = &c.schema.primary().field_name;
            let mut local = vec![];
            let mut next = c.next_id;
            let mut seen = BTreeSet::new();
            for (row, id) in data.iter().zip(ids) {
                c.schema.primary().validate(id)?;
                if !seen.insert(id.to_string()) {
                    return Err(invalid("duplicate committed PK"));
                }
                if c.schema.auto_id && op == "entities/insert" {
                    if row.get(pk).is_some() || id.as_i64() != Some(next) {
                        return Err(invalid("non-deterministic autoID replay"));
                    }
                    next = next
                        .checked_add(1)
                        .ok_or_else(|| invalid("autoID overflow"))?;
                } else {
                    if &row[pk] != id {
                        return Err(invalid("committed primary key mismatch"));
                    }
                    if c.schema.auto_id {
                        next = next.max(
                            id.as_i64()
                                .unwrap()
                                .checked_add(1)
                                .ok_or_else(|| invalid("autoID overflow"))?,
                        );
                    }
                }
                if shard_for(&id.to_string(), count) == shard {
                    let mut row = row.clone();
                    row[pk] = id.clone();
                    local.push(row);
                }
            }
            let mut batch = if local.is_empty() {
                MutationBatch {
                    base: self.revision,
                    spec: self.spec.clone(),
                    rows: vec![],
                    result: json!({}),
                }
            } else {
                let mut r = request.clone();
                r["data"] = json!(local);
                self.prepare(
                    if c.schema.auto_id && op == "entities/insert" {
                        "entities/upsert"
                    } else {
                        op
                    },
                    r,
                )?
            };
            batch.spec.as_mut().unwrap().next_id = next;
            batch
        } else {
            let batch = self.prepare(op, request.clone())?;
            if op == "entities/delete" {
                let n = result["deleteCount"]
                    .as_u64()
                    .ok_or_else(|| invalid("missing delete count"))?;
                if batch.result["deleteCount"].as_u64().unwrap() > n
                    || n > request["ids"]
                        .as_array()
                        .ok_or_else(|| invalid("missing ids"))?
                        .len() as u64
                {
                    return Err(invalid("invalid global delete count"));
                }
            } else if &batch.result != result {
                return Err(invalid("control replay result mismatch"));
            }
            batch
        };
        if let Some(c) = &batch.spec
            && c.shard_count != count
        {
            return Err(Error::Integrity("shard count changed".into()));
        }
        if batch.rows.iter().any(|r| shard_for(&r.key, count) != shard) {
            return Err(Error::Integrity("foreign row in shard delta".into()));
        }
        self.commit(batch, version)?;
        Ok(())
    }
}
