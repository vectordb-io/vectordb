//! Linearizable name catalog and durable request-to-generation bindings.
//! Entity bodies never enter etcd; each receipt stores only a digest and target.
use crate::{
    Error, Result,
    catalog::Catalog,
    state::{IDEMPOTENCY_WINDOW_MS, Scope, expiry, now_ms, request_digest},
};
use base64::{Engine, engine::general_purpose::STANDARD};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::{BTreeMap, BTreeSet};
use vectordb_engine::segment::{CollectionSpec, SegmentEngine};

pub const MAX_COLLECTIONS: usize = 16;
const MAX_IDENTITIES: usize = 256;
pub const MAX_ROUTE_RECEIPTS: usize = 32768;
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum Lifecycle {
    Creating,
    Active,
    Dropping,
    Dropped,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Collection {
    pub database: String,
    pub name: String,
    pub scope: Scope,
    pub state: Lifecycle,
    pub create_request: Value,
    pub create_id: String,
    pub drop_id: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub dropped_at_ms: Option<i64>,
    pub spec: Option<CollectionSpec>,
    pub spec_offset: i64,
    #[serde(default = "vectordb_engine::shard::one")]
    pub shard_count: usize,
    #[serde(default)]
    pub query_routes: Option<crate::sharded::QueryRoutes>,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct DirectoryState {
    pub format: u32,
    pub clock_ms: i64,
    pub databases: BTreeSet<String>,
    #[serde(default)]
    receipt_expirations: BTreeMap<i64, usize>,
    pub collections: BTreeMap<String, Collection>,
    pub aliases: BTreeMap<String, String>,
}
impl Default for DirectoryState {
    fn default() -> Self {
        Self {
            format: 1,
            clock_ms: 0,
            databases: BTreeSet::from(["default".into()]),
            receipt_expirations: BTreeMap::new(),
            collections: BTreeMap::new(),
            aliases: BTreeMap::new(),
        }
    }
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Receipt {
    pub digest: String,
    pub target: Option<Scope>,
    pub result: Option<Value>,
}
#[derive(Clone)]
pub struct Directory {
    pub(crate) store: Catalog,
    cluster: String,
}
fn missing(message: &str) -> Error {
    Error::Engine(vectordb_engine::Error::NotFound(message.into()))
}
fn text<'a>(request: &'a Value, key: &str) -> Result<&'a str> {
    let value = request[key]
        .as_str()
        .ok_or_else(|| Error::Invalid(format!("{key} required")))?;
    crate::identifier(value)?;
    Ok(value)
}
pub fn database(request: &Value) -> Result<&str> {
    let db = request
        .get("dbName")
        .map(|v| {
            v.as_str()
                .ok_or_else(|| Error::Invalid("dbName must be a string".into()))
        })
        .transpose()?
        .unwrap_or("default");
    crate::identifier(db)?;
    Ok(db)
}
fn alias_key(db: &str, name: &str) -> String {
    format!("{db}/{name}")
}
fn receipt_bucket(expires: i64) -> i64 {
    (expires + 60_000).div_euclid(60_000) * 60_000
}
pub fn receipt_name(request: &Value, id: &str) -> Result<String> {
    let issued = id
        .strip_prefix("v2-")
        .and_then(|s| s.split_once('-'))
        .and_then(|(s, _)| s.parse::<i64>().ok())
        .ok_or_else(|| Error::Invalid("invalid idempotency key".into()))?;
    let expires = issued
        .checked_add(IDEMPOTENCY_WINDOW_MS + 60_000)
        .filter(|v| *v >= 60_000)
        .ok_or_else(|| Error::Invalid("invalid idempotency timestamp".into()))?
        - 60_000;
    let name = request
        .get("collectionName")
        .or_else(|| request.get("aliasName"))
        .and_then(Value::as_str)
        .unwrap_or("");
    Ok(format!(
        "requests/{:020}/{}",
        receipt_bucket(expires),
        crate::digest(&serde_json::to_vec(&(database(request)?, name, id))?)
    ))
}

impl DirectoryState {
    fn admit_receipt(&mut self, expires: i64, now: i64) -> Result<()> {
        self.receipt_expirations.retain(|end, _| *end > now);
        if self.receipt_expirations.values().sum::<usize>() >= MAX_ROUTE_RECEIPTS {
            return Err(Error::Unavailable(
                "routing receipt quota exhausted; wait for expiry".into(),
            ));
        }
        // Cleanup and the monotonic clock advance share the receipt's CAS. A wall
        // clock rollback cannot make a deleted binding eligible for reuse.
        let end = receipt_bucket(expires);
        *self.receipt_expirations.entry(end).or_default() += 1;
        Ok(())
    }
    fn named(&self, db: &str, name: &str) -> Option<&Collection> {
        self.collections
            .values()
            .find(|c| c.database == db && c.name == name && c.state != Lifecycle::Dropped)
    }
    pub fn resolve(&self, db: &str, name: &str) -> Result<Collection> {
        if !self.databases.contains(db) {
            return Err(missing("database not found"));
        }
        let collection = self
            .named(db, name)
            .or_else(|| {
                self.aliases
                    .get(&alias_key(db, name))
                    .and_then(|id| self.collections.get(id))
            })
            .ok_or_else(|| missing("collection not found"))?;
        if collection.state != Lifecycle::Active {
            return Err(Error::Conflict(
                "collection lifecycle operation in progress".into(),
            ));
        }
        Ok(collection.clone())
    }
    pub fn by_scope(&self, scope: &Scope) -> Result<Collection> {
        self.collections
            .get(&scope.collection)
            .filter(|c| c.scope == *scope)
            .cloned()
            .ok_or_else(|| missing("collection generation not found"))
    }
    pub fn read(&self, op: &str, r: &Value) -> Result<Value> {
        vectordb_engine::validate_request(op, r)?;
        let db = database(r)?;
        if op == "databases/list" {
            return Ok(json!(self.databases));
        }
        if !self.databases.contains(db) {
            return Err(missing("database not found"));
        }
        match op {
            "databases/describe" => Ok(json!({"dbName":db})),
            "collections/list" => Ok(json!(
                self.collections
                    .values()
                    .filter(|c| c.database == db && c.state == Lifecycle::Active)
                    .map(|c| &c.name)
                    .collect::<BTreeSet<_>>()
            )),
            "collections/has" => {
                Ok(json!({"has":self.resolve(db,text(r,"collectionName")?).is_ok()}))
            }
            "aliases/list" => {
                let c = self.resolve(db, text(r, "collectionName")?)?;
                Ok(json!(
                    self.aliases
                        .iter()
                        .filter(|(_, id)| **id == c.scope.collection)
                        .map(|(key, _)| key.split_once('/').unwrap().1)
                        .collect::<Vec<_>>()
                ))
            }
            "aliases/describe" => {
                let alias = text(r, "aliasName")?;
                let id = self
                    .aliases
                    .get(&alias_key(db, alias))
                    .ok_or_else(|| missing("alias not found"))?;
                let c = self.collections.get(id).ok_or_else(|| {
                    Error::Integrity("alias references missing collection".into())
                })?;
                Ok(
                    json!({"aliasName":alias,"collectionName":c.name,"collectionId":c.scope.collection,"generation":c.scope.generation,"dbName":db}),
                )
            }
            _ => Err(Error::Unsupported(op.into())),
        }
    }
    /// Pure transition, applied by one etcd compare-and-swap together with receipt.
    pub fn plan(&mut self, cluster: &str, op: &str, r: &Value, id: &str) -> Result<Receipt> {
        let db = database(r)?.to_string();
        let digest = request_digest(op, r)?;
        if op != "databases/create" && !self.databases.contains(&db) {
            return Err(missing("database not found"));
        }
        let mut receipt = Receipt {
            digest,
            target: None,
            result: None,
        };
        match op {
            "databases/create" => {
                vectordb_engine::validate_request(op, r)?;
                if self.databases.len() >= 32 {
                    return Err(Error::Conflict("database quota reached".into()));
                }
                if !self.databases.insert(db) {
                    return Err(Error::Conflict("database exists".into()));
                }
                receipt.result = Some(json!({}));
            }
            "databases/drop" => {
                vectordb_engine::validate_request(op, r)?;
                if db == "default"
                    || self
                        .collections
                        .values()
                        .any(|c| c.database == db && c.state != Lifecycle::Dropped)
                {
                    return Err(Error::Conflict(
                        "database must be empty and not default".into(),
                    ));
                }
                self.databases.remove(&db);
                receipt.result = Some(json!({}));
            }
            "collections/create" => {
                let name = text(r, "collectionName")?;
                if self.named(&db, name).is_some()
                    || self.aliases.contains_key(&alias_key(&db, name))
                {
                    return Err(Error::Conflict("collection or alias exists".into()));
                }
                if self.collections.len() >= MAX_IDENTITIES
                    || self
                        .collections
                        .values()
                        .filter(|c| c.state != Lifecycle::Dropped)
                        .count()
                        >= MAX_COLLECTIONS
                {
                    return Err(Error::Conflict("collection catalog quota reached".into()));
                }
                let scope = Scope {
                    cluster: cluster.into(),
                    collection: format!("c{}", uuid::Uuid::new_v4().simple()),
                    generation: uuid::Uuid::new_v4().simple().to_string(),
                };
                let mut create = r.clone();
                if create.get("shardCount").is_none() {
                    create["shardCount"] = json!(16);
                }
                create["dbName"] = json!("default");
                create["collectionName"] = json!(scope.collection);
                let mut engine = SegmentEngine::new();
                let batch = engine.prepare(op, create.clone())?;
                engine.commit(batch, 1)?;
                if serde_json::to_vec(&create)?.len() > 64 * 1024 {
                    return Err(Error::Invalid(
                        "collection definition exceeds 64 KiB".into(),
                    ));
                }
                self.collections.insert(
                    scope.collection.clone(),
                    Collection {
                        database: db,
                        name: name.into(),
                        scope: scope.clone(),
                        state: Lifecycle::Creating,
                        create_request: create,
                        create_id: id.into(),
                        drop_id: None,
                        dropped_at_ms: None,
                        spec: engine.spec().cloned(),
                        spec_offset: -1,
                        shard_count: engine.spec().unwrap().shard_count,
                        query_routes: None,
                    },
                );
                receipt.target = Some(scope);
            }
            "collections/set_query_routes" => {
                let object = r
                    .as_object()
                    .ok_or_else(|| Error::Invalid("request must be object".into()))?;
                if object.keys().any(|k| {
                    !["dbName", "collectionName", "replicas", "expectedEpoch"].contains(&k.as_str())
                }) {
                    return Err(Error::Invalid("unknown route option".into()));
                }
                let target = self.resolve(&db, text(r, "collectionName")?)?;
                let previous = target.query_routes.as_ref().map_or(0, |r| r.epoch);
                if r["expectedEpoch"].as_u64() != Some(previous) {
                    return Err(Error::Conflict("query routing epoch changed".into()));
                }
                let routes = crate::sharded::QueryRoutes {
                    epoch: previous
                        .checked_add(1)
                        .ok_or_else(|| Error::Conflict("route epoch exhausted".into()))?,
                    hash_version: 1,
                    shards: serde_json::from_value(r["replicas"].clone()).map_err(|_| {
                        Error::Invalid("replicas must be an array of address arrays".into())
                    })?,
                };
                routes.validate(target.shard_count)?;
                self.collections
                    .get_mut(&target.scope.collection)
                    .unwrap()
                    .query_routes = Some(routes.clone());
                receipt.result = Some(json!(routes));
            }
            "collections/drop" => {
                vectordb_engine::validate_request(op, r)?;
                let name = text(r, "collectionName")?;
                let c = self
                    .named(&db, name)
                    .ok_or_else(|| missing("collection not found; drop requires canonical name"))?
                    .clone();
                if c.state != Lifecycle::Active {
                    return Err(Error::Conflict("collection transition in progress".into()));
                }
                let entry = self.collections.get_mut(&c.scope.collection).unwrap();
                entry.state = Lifecycle::Dropping;
                entry.drop_id = Some(id.into());
                self.aliases
                    .retain(|_, target| target != &c.scope.collection);
                receipt.target = Some(c.scope);
            }
            "aliases/create" | "aliases/alter" | "aliases/drop" => {
                vectordb_engine::validate_request(op, r)?;
                let alias = text(r, "aliasName")?;
                let key = alias_key(&db, alias);
                if op == "aliases/drop" {
                    self.aliases
                        .remove(&key)
                        .ok_or_else(|| missing("alias not found"))?;
                } else {
                    if self.named(&db, alias).is_some() {
                        return Err(Error::Conflict("alias collides with collection".into()));
                    }
                    if op == "aliases/create" && self.aliases.contains_key(&key) {
                        return Err(Error::Conflict("alias exists".into()));
                    }
                    if op == "aliases/alter" && !self.aliases.contains_key(&key) {
                        return Err(missing("alias not found"));
                    }
                    let name = text(r, "collectionName")?;
                    let c = self
                        .named(&db, name)
                        .filter(|c| c.state == Lifecycle::Active)
                        .ok_or_else(|| missing("active canonical collection required"))?;
                    self.aliases.insert(key, c.scope.collection.clone());
                }
                receipt.result = Some(json!({}));
            }
            _ => {
                receipt.target = Some(self.resolve(&db, text(r, "collectionName")?)?.scope);
            }
        }
        Ok(receipt)
    }
}
impl Directory {
    pub fn new(endpoint: String, cluster: &str) -> Result<Self> {
        crate::identifier(cluster)?;
        // Reserved catalog scope cannot collide with UUID collection identities.
        Ok(Self {
            store: Catalog::new(
                endpoint,
                &Scope {
                    cluster: cluster.into(),
                    collection: "_catalog".into(),
                    generation: "v1".into(),
                },
            )?,
            cluster: cluster.into(),
        })
    }
    pub(crate) fn load(&self) -> Result<(String, DirectoryState)> {
        match self.store.get("directory")? {
            None => Ok(("0".into(), DirectoryState::default())),
            Some((revision, bytes)) => {
                let state: DirectoryState = serde_json::from_slice(&bytes)?;
                if state.format != 1 {
                    return Err(Error::Integrity("unsupported directory format".into()));
                }
                Ok((revision, state))
            }
        }
    }
    pub fn snapshot(&self) -> Result<DirectoryState> {
        Ok(self.load()?.1)
    }
    pub(crate) fn compare(&self, revision: &str) -> Value {
        if revision == "0" {
            json!({"key":self.store.key("directory"),"target":"VERSION","version":"0","result":"EQUAL"})
        } else {
            json!({"key":self.store.key("directory"),"target":"MOD","mod_revision":revision,"result":"EQUAL"})
        }
    }
    pub(crate) fn put(&self, state: &DirectoryState) -> Result<Value> {
        let bytes = serde_json::to_vec(state)?;
        if bytes.len() > 1024 * 1024 {
            return Err(Error::Unavailable(
                "directory metadata quota exhausted".into(),
            ));
        }
        Ok(
            json!({"request_put":{"key":self.store.key("directory"),"value":STANDARD.encode(bytes)}}),
        )
    }
    pub fn backup_receipts(&self) -> Result<BTreeMap<String, Receipt>> {
        let prefix = format!("/vectordb/{}/_catalog/v1/", self.cluster);
        self.store
            .range("requests/")?
            .into_iter()
            .map(|(key, bytes)| {
                Ok((
                    key.strip_prefix(&prefix)
                        .ok_or_else(|| Error::Integrity("foreign receipt".into()))?
                        .to_string(),
                    serde_json::from_slice(&bytes)?,
                ))
            })
            .collect()
    }
    /// The durable reservation keeps partially restored receipt batches invisible after a crash.
    pub fn restore_step(
        &self,
        lease: &crate::catalog::Lease,
        archive: &str,
        receipts: &[(String, Receipt)],
        directory: Option<&DirectoryState>,
    ) -> Result<()> {
        let mut compare = vec![
            self.compare("0"),
            json!({"key":self.store.key("owner"),"target":"VALUE","value":STANDARD.encode(serde_json::to_vec(&lease.owner)?)}),
        ];
        let key = self.store.key("restore");
        match self.store.get("restore")? {
            Some((_, id)) if id == archive.as_bytes() => {
                compare.push(json!({"key":key,"target":"VALUE","value":STANDARD.encode(archive)}))
            }
            None => compare.push(json!({"key":key,"target":"VERSION","version":"0"})),
            _ => return Err(Error::Conflict("target reserved by another archive".into())),
        }
        let mut success = vec![];
        for (key, receipt) in receipts {
            let parts: Vec<_> = key.split('/').collect();
            if parts.len() != 3
                || parts[0] != "requests"
                || parts[1].len() != 20
                || !parts[1].bytes().all(|b| b.is_ascii_digit())
                || parts[2].len() != 64
                || !parts[2].bytes().all(|b| b.is_ascii_hexdigit())
            {
                return Err(Error::Integrity("invalid receipt key".into()));
            }
            success.push(json!({"request_put":{"key":self.store.key(key),"value":STANDARD.encode(serde_json::to_vec(receipt)?)}}));
        }
        if let Some(state) = directory {
            success.push(self.put(state)?);
            success.push(json!({"request_delete_range":{"key":key}}));
        } else {
            success.push(json!({"request_put":{"key":key,"value":STANDARD.encode(archive)}}));
        }
        let r = self
            .store
            .post("/v3/kv/txn", json!({"compare":compare,"success":success}))?;
        if r["succeeded"] != true {
            return Err(Error::Conflict(
                "restore requires an empty target and valid reservation".into(),
            ));
        }
        Ok(())
    }
    pub fn maintenance_store(&self) -> Catalog {
        self.store.clone()
    }
    pub fn allow_mutations(&self) -> Result<()> {
        if self.store.get("owner")?.is_some() || self.store.get("restore")?.is_some() {
            return Err(Error::Unavailable(
                "cluster write freeze in progress".into(),
            ));
        }
        Ok(())
    }
    pub fn configure_quota(&self, limit: usize) -> Result<()> {
        if !(1..=MAX_COLLECTIONS).contains(&limit) {
            return Err(Error::Invalid(
                "namespace collection limit outside supported range".into(),
            ));
        }
        let key = self.store.key("namespace-limit");
        self.store.post("/v3/kv/txn", json!({"compare":[{"key":key,"target":"VERSION","version":"0"}],"success":[{"request_put":{"key":key,"value":STANDARD.encode(limit.to_string())}}]}))?;
        if self.namespace_limit()? != limit {
            return Err(Error::Conflict(
                "namespace limit differs from cluster policy".into(),
            ));
        }
        Ok(())
    }
    fn namespace_limit(&self) -> Result<usize> {
        match self.store.get("namespace-limit")? {
            Some((_, bytes)) => Ok(serde_json::from_slice(&bytes)?),
            None => Ok(MAX_COLLECTIONS),
        }
    }
    pub fn begin(&self, op: &str, r: &Value, id: &str) -> Result<Receipt> {
        self.begin_at(op, r, id, now_ms())
    }
    pub fn begin_at(&self, op: &str, r: &Value, id: &str, at_ms: i64) -> Result<Receipt> {
        let db = database(r)?;
        let key = receipt_name(r, id)?;
        let hash = request_digest(op, r)?;
        for _ in 0..32 {
            let (revision, mut state) = self.load()?;
            let now = at_ms.max(state.clock_ms);
            let expires = expiry(id, now)?;
            if let Some((_, bytes)) = self.store.get(&key)? {
                self.allow_mutations()?;
                let receipt: Receipt = serde_json::from_slice(&bytes)?;
                if receipt.digest == "retired-p1-key" {
                    return Err(Error::Conflict(
                        "P1 request key retired during offline migration".into(),
                    ));
                }
                if receipt.digest != hash {
                    return Err(Error::Conflict(
                        "idempotency key reused with different request".into(),
                    ));
                }
                return Ok(receipt);
            }
            if op == "collections/create"
                && state
                    .collections
                    .values()
                    .filter(|c| c.database == db && c.state != Lifecycle::Dropped)
                    .count()
                    >= self.namespace_limit()?
            {
                return Err(Error::Conflict("namespace collection quota reached".into()));
            }
            state.admit_receipt(expires, now)?;
            let receipt = state.plan(&self.cluster, op, r, id)?;
            state.clock_ms = now;
            // Leave space for terminal transitions even when admitting new names.
            if matches!(
                op,
                "collections/create" | "databases/create" | "aliases/create"
            ) && serde_json::to_vec(&state)?.len() > 900 * 1024
            {
                return Err(Error::Unavailable(
                    "directory admission quota exhausted".into(),
                ));
            }
            let put = self.put(&state)?;
            let end = format!("requests/{:020}/", now.div_euclid(60_000) * 60_000 + 1);
            let result=self.store.post("/v3/kv/txn",json!({"compare":[self.compare(&revision),{"key":self.store.key("owner"),"target":"VERSION","version":"0"},{"key":self.store.key("restore"),"target":"VERSION","version":"0"},{"key":self.store.key(&key),"target":"VERSION","version":"0","result":"EQUAL"}],"success":[put,{"request_delete_range":{"key":self.store.key("requests/"),"range_end":self.store.key(&end)}},{"request_put":{"key":self.store.key(&key),"value":STANDARD.encode(serde_json::to_vec(&receipt)?)}}],"failure":[]}))?;
            if result["succeeded"] == true {
                return Ok(receipt);
            }
            self.allow_mutations()?;
        }
        Err(Error::Unavailable(
            "catalog contention; retry same request".into(),
        ))
    }
    pub fn finish(
        &self,
        scope: &Scope,
        state: Lifecycle,
        spec: Option<CollectionSpec>,
        offset: i64,
    ) -> Result<()> {
        for _ in 0..32 {
            let (revision, mut directory) = self.load()?;
            let c = directory
                .collections
                .get_mut(&scope.collection)
                .filter(|c| c.scope == *scope)
                .ok_or_else(|| missing("generation not found"))?;
            if c.state == Lifecycle::Dropped {
                return Ok(());
            }
            if state == Lifecycle::Active && c.state == Lifecycle::Dropping {
                return Ok(());
            }
            if state == Lifecycle::Dropped && c.state != Lifecycle::Dropping {
                return Err(Error::Conflict("drop was not prepared".into()));
            }
            c.state = state;
            if offset >= c.spec_offset {
                c.spec = spec.clone();
                c.spec_offset = offset;
            }
            if state == Lifecycle::Dropped {
                c.dropped_at_ms = Some(now_ms().max(directory.clock_ms));
                c.create_request = json!({});
                c.spec = None;
            }
            // Schema/index/partition snapshots are derived, versioned caches. WAL and
            // immutable create intent remain authoritative when their cache is evicted.
            if serde_json::to_vec(&directory)?.len() > 950 * 1024 {
                for collection in directory.collections.values_mut() {
                    collection.spec = None;
                }
            }
            let result=self.store.post("/v3/kv/txn",json!({"compare":[self.compare(&revision)],"success":[self.put(&directory)?],"failure":[]}))?;
            if result["succeeded"] == true {
                return Ok(());
            }
        }
        Err(Error::Unavailable(
            "catalog publication raced; retry".into(),
        ))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn route_receipt_quota_releases_only_after_expiry_margin() {
        let mut state = DirectoryState::default();
        for _ in 0..MAX_ROUTE_RECEIPTS {
            state.admit_receipt(60_000, 0).unwrap();
        }
        assert!(state.admit_receipt(90_000, 60_000).is_err());
        state.admit_receipt(180_000, 120_000).unwrap();
        assert_eq!(state.receipt_expirations.values().sum::<usize>(), 1);
    }
}
