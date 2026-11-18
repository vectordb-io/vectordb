//! Immutable prepare records and the compact collection-level commit decision.
use crate::{
    Error, Result, digest,
    state::{Entry, FORMAT, Record, Scope, request_digest},
};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::BTreeMap;

const MAX_ROWS: usize = 10_000;
pub const MAX_CHANNELS: usize = 16;

pub fn validate_count(count: usize) -> Result<()> {
    if count.is_power_of_two() && count <= MAX_CHANNELS {
        Ok(())
    } else {
        Err(Error::Invalid(
            "writeChannels must be 1, 2, 4, 8 or 16".into(),
        ))
    }
}
pub fn validate_cut(previous: &[i64], floors: &[i64]) -> Result<()> {
    validate_count(floors.len())?;
    if (!previous.is_empty() && previous.len() != floors.len())
        || floors
            .iter()
            .enumerate()
            .any(|(i, n)| *n < previous.get(i).copied().unwrap_or(0))
    {
        return Err(Error::Integrity(
            "regressing or incompatible participant cut".into(),
        ));
    }
    Ok(())
}
pub fn field(operation: &str) -> Option<&'static str> {
    match operation {
        "entities/insert" | "entities/upsert" => Some("data"),
        "entities/delete" => Some("ids"),
        _ => None,
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Part {
    pub channel: i32,
    pub offset: i64,
    pub sha256: String,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Prepared {
    pub request_id: String,
    pub operation: String,
    pub hash: String,
    pub header: Value,
    pub count: usize,
    pub parts: Vec<Part>,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Fragment {
    scope: Scope,
    hash: String,
    request_id: String,
    channel: i32,
    rows: Vec<(usize, Value)>,
}

impl Prepared {
    pub fn split(
        scope: &Scope,
        channels: usize,
        primary: &str,
        id: &str,
        operation: &str,
        mut request: Value,
    ) -> Result<(Self, BTreeMap<i32, Vec<u8>>)> {
        validate_count(channels)?;
        let field =
            field(operation).ok_or_else(|| Error::Invalid("expected entity mutation".into()))?;
        let hash = request_digest(operation, &request)?;
        let Some(Value::Array(rows)) = request.as_object_mut().and_then(|o| o.remove(field)) else {
            return Err(Error::Invalid(format!("{field} must be an array")));
        };
        let count = rows.len();
        if !(1..=MAX_ROWS).contains(&count) {
            return Err(Error::Invalid("invalid batch length".into()));
        }
        let mut groups = BTreeMap::<i32, Vec<(usize, Value)>>::new();
        let start = vectordb_engine::shard::shard_for(id, channels);
        for (i, row) in rows.into_iter().enumerate() {
            let key = if field == "ids" {
                Some(&row)
            } else {
                row.get(primary)
            };
            let channel = key.map_or((i + start) % channels, |pk| {
                vectordb_engine::shard::shard_for(&pk.to_string(), channels)
            }) as i32;
            groups.entry(channel).or_default().push((i, row));
        }
        let mut fragments = BTreeMap::new();
        for (channel, rows) in groups {
            let fragment = Fragment {
                scope: scope.clone(),
                hash: hash.clone(),
                request_id: id.into(),
                channel,
                rows,
            };
            fragments.insert(channel, serde_json::to_vec(&fragment)?);
        }
        Ok((
            Self {
                request_id: id.into(),
                operation: operation.into(),
                hash,
                header: request,
                count,
                parts: vec![],
            },
            fragments,
        ))
    }
    pub fn validate(&self, scope: &Scope, channels: usize) -> Result<()> {
        validate_count(channels)?;
        let field = field(&self.operation)
            .ok_or_else(|| Error::Integrity("invalid participant operation".into()))?;
        crate::identifier(&self.request_id)?;
        if self.header["collectionName"] != scope.collection
            || self.header.get(field).is_some()
            || !(1..=MAX_ROWS).contains(&self.count)
            || self.parts.is_empty()
            || self.parts.len() > channels
            || self.parts.windows(2).any(|p| p[0].channel >= p[1].channel)
            || self.parts.iter().any(|p| {
                p.offset < 0 || !(0..channels as i32).contains(&p.channel) || p.sha256.len() != 64
            })
        {
            return Err(Error::Integrity(
                "invalid transaction participant list".into(),
            ));
        }
        Ok(())
    }
    pub fn above_cut(&self, floors: &[i64]) -> bool {
        self.parts
            .iter()
            .all(|p| p.offset >= floors.get(p.channel as usize).copied().unwrap_or(0))
    }
    pub fn assemble(&self, scope: &Scope, mut payloads: BTreeMap<i32, Vec<u8>>) -> Result<Value> {
        self.validate(scope, MAX_CHANNELS)?;
        if payloads.len() != self.parts.len() {
            return Err(Error::Integrity(
                "incomplete transaction participants".into(),
            ));
        }
        let mut rows = vec![None; self.count];
        for part in &self.parts {
            let bytes = payloads
                .remove(&part.channel)
                .ok_or_else(|| Error::Integrity("missing prepare".into()))?;
            if digest(&bytes) != part.sha256 {
                return Err(Error::Integrity("prepare digest mismatch".into()));
            }
            let f: Fragment = serde_json::from_slice(&bytes)?;
            if f.scope != *scope
                || f.hash != self.hash
                || f.request_id != self.request_id
                || f.channel != part.channel
                || f.rows.is_empty()
            {
                return Err(Error::Integrity("foreign prepare".into()));
            }
            for (i, v) in f.rows {
                if rows.get_mut(i).is_none_or(|slot| slot.replace(v).is_some()) {
                    return Err(Error::Integrity("invalid prepare ordinal".into()));
                }
            }
        }
        let mut request = self.header.clone();
        request[field(&self.operation).unwrap()] = Value::Array(
            rows.into_iter()
                .collect::<Option<Vec<_>>>()
                .ok_or_else(|| Error::Integrity("incomplete prepared batch".into()))?,
        );
        if request_digest(&self.operation, &request)? != self.hash {
            return Err(Error::Integrity("transaction digest mismatch".into()));
        }
        Ok(request)
    }
}
/// Format 3 appears only in an opted-in generation's decision WAL.
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Decision {
    pub format: u32,
    pub scope: Scope,
    pub epoch: i64,
    pub accepted_ms: i64,
    pub result: Value,
    pub prepared: Prepared,
}
impl Decision {
    pub fn resolve(self, request: Value) -> Result<Record> {
        if self.format != 3
            || request_digest(&self.prepared.operation, &request)? != self.prepared.hash
        {
            return Err(Error::Integrity("invalid commit decision".into()));
        }
        Ok(self.record(request))
    }

    pub(crate) fn record(&self, request: Value) -> Record {
        Record {
            format: FORMAT,
            scope: self.scope.clone(),
            entry: Entry::Mutation {
                epoch: self.epoch,
                accepted_ms: self.accepted_ms,
                request_id: self.prepared.request_id.clone(),
                operation: self.prepared.operation.clone(),
                request,
                result: self.result.clone(),
                digest: self.prepared.hash.clone(),
            },
        }
    }
}

/// A writer peer signs only after Kafka acknowledges every part. The owner can
/// decide using keys and exact quota charges without receiving vector payloads.
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Vote {
    pub prepared: Prepared,
    pub intents: Vec<vectordb_engine::segment::RowIntent>,
    pub request_bytes: usize,
    mac: Vec<u8>,
}
impl Vote {
    fn signer(&self, scope: &Scope, token: &str) -> Result<hmac::Hmac<sha2::Sha256>> {
        use hmac::Mac;
        let mut mac =
            hmac::Hmac::new_from_slice(token.as_bytes()).expect("HMAC accepts arbitrary keys");
        mac.update(b"vectordb/write-prepare/v1");
        mac.update(&serde_json::to_vec(&(
            scope,
            &self.prepared,
            &self.intents,
            self.request_bytes,
        ))?);
        Ok(mac)
    }
    pub fn sign(
        scope: &Scope,
        token: &str,
        prepared: Prepared,
        intents: Vec<vectordb_engine::segment::RowIntent>,
        request_bytes: usize,
    ) -> Result<Self> {
        use hmac::Mac;
        let mut vote = Self {
            prepared,
            intents,
            request_bytes,
            mac: vec![],
        };
        vote.mac = vote.signer(scope, token)?.finalize().into_bytes().to_vec();
        Ok(vote)
    }
    pub fn verify(&self, scope: &Scope, channels: usize, token: &str) -> Result<()> {
        use hmac::Mac;
        self.prepared.validate(scope, channels)?;
        self.signer(scope, token)?
            .verify_slice(&self.mac)
            .map_err(|_| Error::Integrity("invalid writer prepare attestation".into()))?;
        if self.intents.len() != self.prepared.count || self.request_bytes > 1024 * 1024 {
            return Err(Error::Integrity("invalid prepare summary".into()));
        }
        Ok(())
    }
}
