//! Bounded primary-key overlay between the materialized prefix and the decision head.
//! The authoritative history is the Kafka decision WAL, never this disposable cache.
use crate::{
    Error, Result,
    channels::{Decision, Vote},
    state::{MAX_DEDUP_BYTES, Replica, Token, WriteResult, expiry},
};
use serde_json::{Value, json};
use std::collections::{BTreeMap, BTreeSet};
use vectordb_engine::segment::VersionedRow;

#[derive(Clone)]
struct KeyState {
    live: bool,
    partition: String,
    bytes: usize,
}
#[derive(Default)]
pub struct WriteHead {
    keys: BTreeMap<String, KeyState>,
    receipts: BTreeMap<String, (String, WriteResult, usize)>,
    pub offset: Option<i64>,
    bytes: usize,
    next_id: i64,
    clock: i64,
}
pub struct Plan {
    pub decision: Decision,
    changes: BTreeMap<String, KeyState>,
    bytes: usize,
    next_id: i64,
}
impl WriteHead {
    pub fn next_id(&self) -> Option<i64> {
        self.offset.map(|_| self.next_id)
    }
    pub fn bytes(&self) -> Option<usize> {
        self.offset.map(|_| self.bytes)
    }
    pub fn prepare(
        &self,
        replica: &Replica,
        vote: Vote,
        limit: usize,
        now: i64,
    ) -> Result<std::result::Result<Plan, WriteResult>> {
        let p = vote.prepared;
        let now = now.max(self.clock).max(replica.clock());
        expiry(&p.request_id, now)?;
        if let Some((hash, r, _)) = self.receipts.get(&p.request_id) {
            if hash != &p.hash {
                return Err(Error::Conflict(
                    "idempotency key reused with different request".into(),
                ));
            }
            return Ok(Err(r.clone()));
        }
        if let Some(r) = replica.known(&p.request_id, &p.hash)? {
            return Ok(Err(r));
        }
        if !p.above_cut(&replica.channel_floors) {
            return Err(Error::Unavailable(
                "prepare passed its checkpoint cut; retry with the same key".into(),
            ));
        }
        let mut spec = replica
            .spec()
            .ok_or_else(|| Error::Conflict("collection dropped".into()))?
            .clone();
        let mut bytes = if self.offset.is_some() {
            self.bytes
        } else {
            replica.state_bytes()
        };
        if self.offset.is_some() {
            spec.next_id = self.next_id;
        }
        let partition = p.header["partitionName"].as_str().unwrap_or("_default");
        if !spec.partitions.contains(partition) {
            return Err(vectordb_engine::Error::NotFound("partition not found".into()).into());
        }
        let delete = p.operation == "entities/delete";
        let insert = p.operation == "entities/insert";
        let mut ids = Vec::new();
        let mut seen = BTreeSet::new();
        let mut changes = BTreeMap::new();
        for intent in vote.intents {
            let generated = insert && spec.schema.auto_id;
            let id = if generated {
                if intent.id.is_some() {
                    return Err(Error::Integrity("unexpected prepared primary key".into()));
                }
                json!(spec.next_id)
            } else {
                intent
                    .id
                    .ok_or_else(|| Error::Integrity("missing prepared primary key".into()))?
            };
            spec.schema.primary().validate(&id)?;
            if !delete && spec.schema.auto_id {
                let n = id
                    .as_i64()
                    .ok_or_else(|| Error::Integrity("invalid autoID".into()))?;
                if n >= spec.next_id {
                    spec.next_id = n
                        .checked_add(1)
                        .ok_or_else(|| Error::Invalid("autoID exhausted".into()))?;
                }
            }
            let key = id.to_string();
            if !seen.insert(key.clone()) {
                return Err(Error::Invalid("duplicate primary key within batch".into()));
            }
            let old = self.keys.get(&key).cloned().or_else(|| {
                replica.row_by_key(&key).map(|r| KeyState {
                    live: r.body.is_some(),
                    partition: r.partition.clone(),
                    bytes: r.bytes(),
                })
            });
            if delete && !old.as_ref().is_some_and(|r| r.live) {
                continue;
            }
            if insert && old.as_ref().is_some_and(|r| r.live) {
                return Err(Error::Conflict("primary key already exists".into()));
            }
            let part = if delete {
                old.as_ref().unwrap().partition.as_str()
            } else {
                partition
            };
            let body = if generated {
                intent.body_bytes + key.len() - 1
            } else {
                intent.body_bytes
            };
            let charge = VersionedRow::storage_bytes(key.len(), part.len(), body);
            bytes = bytes - old.as_ref().map_or(0, |r| r.bytes) + charge;
            changes.insert(
                key,
                KeyState {
                    live: !delete,
                    partition: part.into(),
                    bytes: charge,
                },
            );
            ids.push(id);
        }
        spec.check_storage_limit(&p.operation, bytes, limit)?;
        let result = if delete {
            json!({"deleteCount":ids.len()})
        } else if insert {
            json!({"insertCount":ids.len(),"insertIds":ids})
        } else {
            json!({"upsertCount":ids.len(),"upsertIds":ids})
        };
        let retained = replica.retained_bytes(now)
            + self
                .receipts
                .iter()
                .filter(|(id, _)| expiry(id, now).is_ok())
                .map(|(_, v)| v.2)
                .sum::<usize>();
        if retained + p.request_id.len() + serde_json::to_vec(&result)?.len() + 1024
            > MAX_DEDUP_BYTES
        {
            return Err(Error::Unavailable(
                "idempotency state quota exhausted; wait for expiry".into(),
            ));
        }
        let decision = Decision {
            format: 3,
            scope: replica.scope.clone(),
            epoch: replica.epoch,
            accepted_ms: now,
            result,
            prepared: p,
        };
        // Match the size contract that independent full replay will enforce.
        let stub = decision.record(Value::Null);
        if serde_json::to_vec(&stub)?.len() - 4 + vote.request_bytes > 1024 * 1024 {
            return Err(Error::Invalid("WAL entry exceeds 1 MiB".into()));
        }
        Ok(Ok(Plan {
            decision,
            changes,
            bytes,
            next_id: spec.next_id,
        }))
    }
    pub fn committed(&mut self, plan: Plan, offset: i64) -> Result<WriteResult> {
        let d = plan.decision;
        let result = WriteResult {
            data: d.result,
            token: Token {
                scope: d.scope,
                offset,
            },
        };
        self.clock = d.accepted_ms;
        self.offset = Some(offset);
        self.next_id = plan.next_id;
        self.bytes = plan.bytes;
        self.keys.extend(plan.changes);
        let charge = d.prepared.request_id.len()
            + serde_json::to_vec(&(&d.prepared.hash, &result))?.len()
            + 8;
        self.receipts.insert(
            d.prepared.request_id,
            (d.prepared.hash, result.clone(), charge),
        );
        Ok(result)
    }
}
