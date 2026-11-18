//! Durable desired state, lease-fenced reconciliation, and failure-domain placement.
use crate::{
    Error, Result,
    catalog::{Catalog, Lease},
    directory::{Collection, Directory, Lifecycle},
    sharded::QueryRoutes,
    state::{Scope, now_ms},
};
use base64::{Engine, engine::general_purpose::STANDARD};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::{BTreeMap, BTreeSet};
pub const PROTOCOL: u32 = 1;
pub const MAX_NODES: usize = 32;
fn protocol_v1() -> u32 {
    1
}
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum NodeRole {
    Coordinator,
    Writer,
    Query,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Member {
    pub id: String,
    pub incarnation: String,
    pub role: NodeRole,
    pub zone: String,
    pub origin: String,
    pub rpc: String,
    pub protocol_min: u32,
    pub protocol_max: u32,
    pub max_shards: usize,
    pub max_collections: usize,
    pub resources: Value,
}
impl Member {
    pub fn validate(&self) -> Result<()> {
        crate::identifier(&self.id)?;
        if self.id.len() > 63
            || self.id.starts_with('-')
            || self.id.ends_with('-')
            || !self
                .id
                .bytes()
                .all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'-')
        {
            return Err(Error::Invalid(
                "node ID must be a lowercase DNS label matching certificate SAN".into(),
            ));
        }
        crate::identifier(&self.zone)?;
        crate::identifier(&self.incarnation)?;
        if self.protocol_min > PROTOCOL
            || self.protocol_max < PROTOCOL
            || self.protocol_min > self.protocol_max
        {
            return Err(Error::Unsupported(
                "node protocol range is incompatible".into(),
            ));
        }
        for (url, secure) in [(&self.origin, false), (&self.rpc, true)] {
            let u = reqwest::Url::parse(url)
                .map_err(|_| Error::Invalid("invalid node address".into()))?;
            if (secure && u.scheme() != "https")
                || !["http", "https"].contains(&u.scheme())
                || u.host_str().is_none()
                || u.path() != "/"
                || url.ends_with('/')
                || !u.username().is_empty()
                || u.password().is_some()
                || u.query().is_some()
                || u.fragment().is_some()
            {
                return Err(Error::Invalid(
                    "node address must be an origin; RPC requires TLS".into(),
                ));
            }
        }
        if !(1..=512).contains(&self.max_shards)
            || !(1..=16).contains(&self.max_collections)
            || serde_json::to_vec(self)?.len() > 32 * 1024
        {
            return Err(Error::Invalid("invalid node capacity/metadata".into()));
        }
        Ok(())
    }
}
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum Phase {
    Pending,
    Warming,
    Active,
}
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct Placement {
    pub writer: String,
    pub replicas: Vec<Vec<String>>,
    pub incarnations: BTreeMap<String, String>,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Assignment {
    pub scope: Scope,
    pub version: u64,
    pub route_epoch: u64,
    pub placement: Placement,
    pub phase: Phase,
    pub attempts: u32,
    pub retry_at_ms: i64,
    pub error: Option<String>,
    pub maintenance_sequence: u64,
    pub maintenance_pending: bool,
    pub maintenance_completed_ms: i64,
    pub warmed_offset: Option<i64>,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ControlState {
    pub format: u32,
    #[serde(default = "protocol_v1")]
    pub protocol: u32,
    pub replicas: usize,
    pub maintenance_ms: i64,
    pub assignments: BTreeMap<String, Assignment>,
    pub draining: BTreeSet<String>,
    pub leader_epoch: i64,
    pub reconciliations: u64,
    pub last_progress_ms: i64,
}
impl Default for ControlState {
    fn default() -> Self {
        Self {
            format: 1,
            protocol: PROTOCOL,
            replicas: 2,
            maintenance_ms: 60_000,
            assignments: BTreeMap::new(),
            draining: BTreeSet::new(),
            leader_epoch: 0,
            reconciliations: 0,
            last_progress_ms: 0,
        }
    }
}
#[derive(Clone)]
pub struct Control {
    pub(crate) store: Catalog,
}
fn compare(store: &Catalog, name: &str, revision: &str) -> Value {
    if revision == "0" {
        json!({"key":store.key(name),"target":"VERSION","version":"0","result":"EQUAL"})
    } else {
        json!({"key":store.key(name),"target":"MOD","mod_revision":revision,"result":"EQUAL"})
    }
}
impl Control {
    pub fn new(endpoint: String, cluster: &str) -> Result<Self> {
        crate::identifier(cluster)?;
        Ok(Self {
            store: Catalog::new(
                endpoint,
                &Scope {
                    cluster: cluster.into(),
                    collection: "_control".into(),
                    generation: "v1".into(),
                },
            )?,
        })
    }
    pub fn load(&self) -> Result<(String, ControlState)> {
        match self.store.get("state")? {
            Some((rev, bytes)) => {
                let state: ControlState = serde_json::from_slice(&bytes)?;
                if state.format != 1
                    || state.protocol != PROTOCOL
                    || !(1..=3).contains(&state.replicas)
                {
                    return Err(Error::Integrity("unsupported controller state".into()));
                }
                Ok((rev, state))
            }
            None => Ok(("0".into(), ControlState::default())),
        }
    }
    pub fn initialize(&self, replicas: usize, maintenance_ms: i64) -> Result<()> {
        if !(1..=3).contains(&replicas) || !(1000..=3_600_000).contains(&maintenance_ms) {
            return Err(Error::Invalid("invalid control policy".into()));
        }
        let (rev, mut state) = self.load()?;
        if rev != "0" {
            if state.replicas != replicas || state.maintenance_ms != maintenance_ms {
                return Err(Error::Conflict(
                    "coordinator policy differs from persisted policy".into(),
                ));
            }
            return Ok(());
        }
        state.replicas = replicas;
        state.maintenance_ms = maintenance_ms;
        self.store.post("/v3/kv/txn", json!({"compare":[compare(&self.store,"state","0")],"success":[self.state_put(&state)?],"failure":[]}))?;
        let (_, actual) = self.load()?;
        if actual.replicas != replicas || actual.maintenance_ms != maintenance_ms {
            return Err(Error::Conflict(
                "control policy initialization raced".into(),
            ));
        }
        Ok(())
    }
    fn state_put(&self, state: &ControlState) -> Result<Value> {
        let bytes = serde_json::to_vec(state)?;
        if bytes.len() > 1024 * 1024 {
            return Err(Error::Unavailable("control metadata quota".into()));
        }
        Ok(json!({"request_put":{"key":self.store.key("state"),"value":STANDARD.encode(bytes)}}))
    }
    pub fn members(&self) -> Result<BTreeMap<String, Member>> {
        let mut members = BTreeMap::new();
        for (_, bytes) in self.store.range("nodes/")? {
            let value: Value = serde_json::from_slice(&bytes)?;
            let min = value["protocol_min"]
                .as_u64()
                .ok_or_else(|| Error::Integrity("member protocol absent".into()))?;
            let max = value["protocol_max"]
                .as_u64()
                .ok_or_else(|| Error::Integrity("member protocol absent".into()))?;
            if min > max {
                return Err(Error::Integrity("invalid member protocol range".into()));
            }
            // An incompatible registration must not take compatible peers offline.
            if min > u64::from(PROTOCOL) || max < u64::from(PROTOCOL) {
                continue;
            }
            let m: Member = serde_json::from_value(value)?;
            m.validate()?;
            members.insert(m.id.clone(), m);
        }
        Ok(members)
    }
    pub fn register(&self, member: &Member) -> Result<String> {
        member.validate()?;
        self.load()?; // Refuse a cluster with a different persisted wire protocol.
        let grant = self.store.post("/v3/lease/grant", json!({"TTL":10}))?;
        let lease = grant["ID"]
            .as_str()
            .ok_or_else(|| Error::Unavailable("membership lease absent".into()))?
            .to_string();
        let result = (|| {
            for _ in 0..8 {
                let rev = self.store.get("membership")?.map_or("0".into(), |(r, _)| r);
                let members = self.members()?;
                if self.store.range("nodes/")?.len() >= MAX_NODES
                    || members.contains_key(&member.id)
                    || members
                        .values()
                        .any(|m| m.origin == member.origin || m.rpc == member.rpc)
                {
                    return Err(Error::Conflict(
                        "node ID/address occupied or node quota reached".into(),
                    ));
                }
                let value = self.store.post("/v3/kv/txn", json!({"compare":[compare(&self.store,"membership",&rev),compare(&self.store,&format!("nodes/{}",member.id),"0")],"success":[{"request_put":{"key":self.store.key(&format!("nodes/{}",member.id)),"value":STANDARD.encode(serde_json::to_vec(member)?),"lease":lease}},{"request_put":{"key":self.store.key("membership"),"value":STANDARD.encode(&member.incarnation)}},{"request_put":{"key":self.store.key(&format!("incarnations/{}",member.id)),"value":STANDARD.encode(&member.incarnation),"lease":lease}}],"failure":[]}))?;
                if value["succeeded"] == true {
                    return Ok(());
                }
            }
            Err(Error::Unavailable("membership registration raced".into()))
        })();
        if let Err(e) = result {
            let _ = self.store.revoke(&lease);
            return Err(e);
        }
        Ok(lease)
    }
    pub fn heartbeat(&self, member: &Member, lease: &str) -> Result<()> {
        self.store.keepalive(lease)?;
        let key = format!("nodes/{}", member.id);
        let (rev, bytes) = self
            .store
            .get(&key)?
            .ok_or_else(|| Error::Unavailable("membership expired; restart node".into()))?;
        let previous: Member = serde_json::from_slice(&bytes)?;
        if previous.incarnation != member.incarnation {
            return Err(Error::Conflict("node incarnation replaced".into()));
        }
        member.validate()?;
        let r = self.store.post("/v3/kv/txn", json!({"compare":[compare(&self.store,&key,&rev)],"success":[{"request_put":{"key":self.store.key(&key),"value":STANDARD.encode(serde_json::to_vec(member)?),"lease":lease}}],"failure":[]}))?;
        if r["succeeded"] != true {
            return Err(Error::Unavailable("membership heartbeat raced".into()));
        }
        Ok(())
    }
    pub fn verify_member(&self, id: &str, incarnation: &str) -> Result<Member> {
        crate::identifier(id)?;
        crate::identifier(incarnation)?;
        let (_, bytes) = self
            .store
            .get(&format!("nodes/{id}"))?
            .ok_or_else(|| Error::Unavailable("node membership absent".into()))?;
        let member: Member = serde_json::from_slice(&bytes)?;
        if member.incarnation != incarnation {
            return Err(Error::Unavailable("node incarnation expired".into()));
        }
        member.validate()?;
        Ok(member)
    }
    pub fn acquire(&self, id: &str, incarnation: &str) -> Result<Lease> {
        if self.verify_member(id, incarnation)?.role != NodeRole::Coordinator {
            return Err(Error::Invalid("coordinator membership required".into()));
        }
        self.store.acquire(format!("{id}/{incarnation}"))
    }
    pub fn renew(&self, lease: &Lease) -> Result<()> {
        let (id, incarnation) = Self::leader_identity(lease)?;
        self.verify_member(id, incarnation)?;
        self.store.renew(lease)
    }
    pub fn revoke(&self, id: &str) -> Result<()> {
        self.store.revoke(id)
    }
    fn leader_identity(lease: &Lease) -> Result<(&str, &str)> {
        lease
            .owner
            .address
            .split_once('/')
            .ok_or_else(|| Error::Integrity("invalid coordinator identity".into()))
    }
    pub fn leader(&self) -> Result<String> {
        let owner = self.store.owner()?;
        let (id, incarnation) = owner
            .address
            .split_once('/')
            .ok_or_else(|| Error::Integrity("invalid coordinator identity".into()))?;
        self.verify_member(id, incarnation)?;
        Ok(id.into())
    }
    pub fn verify_leader(&self, id: &str, incarnation: &str, epoch: i64) -> Result<()> {
        self.verify_member(id, incarnation)?;
        let (revision, bytes) = self
            .store
            .get("owner")?
            .ok_or_else(|| Error::Unavailable("coordinator lease absent".into()))?;
        let owner: crate::catalog::Owner = serde_json::from_slice(&bytes)?;
        if owner.address != format!("{id}/{incarnation}")
            || revision.parse::<i64>().ok() != Some(epoch)
        {
            return Err(Error::Conflict("coordinator fenced".into()));
        }
        Ok(())
    }
    fn leader_member_compare(&self, lease: &Lease) -> Result<Value> {
        let (id, incarnation) = Self::leader_identity(lease)?;
        Ok(self.incarnation_compare(id, incarnation))
    }
    pub fn incarnation_compare(&self, id: &str, incarnation: &str) -> Value {
        json!({"key":self.store.key(&format!("incarnations/{id}")),"target":"VALUE","value":STANDARD.encode(incarnation),"result":"EQUAL"})
    }
    pub fn leader_compare(&self, lease: &Lease) -> Result<Value> {
        Ok(
            json!({"key":self.store.key("owner"),"target":"VALUE","value":STANDARD.encode(serde_json::to_vec(&lease.owner)?),"result":"EQUAL"}),
        )
    }
    pub fn save(&self, lease: &Lease, revision: &str, state: &mut ControlState) -> Result<bool> {
        state.leader_epoch = lease.epoch;
        state.reconciliations += 1;
        state.last_progress_ms = now_ms();
        let r = self.store.post("/v3/kv/txn", json!({"compare":[self.leader_compare(lease)?,self.leader_member_compare(lease)?,compare(&self.store,"state",revision)],"success":[self.state_put(state)?],"failure":[]}))?;
        Ok(r["succeeded"] == true)
    }
    pub fn assignment(&self, scope: &Scope) -> Result<Assignment> {
        self.load()?
            .1
            .assignments
            .remove(&scope.collection)
            .filter(|a| a.scope == *scope)
            .ok_or_else(|| Error::Unavailable("collection assignment pending".into()))
    }
    pub fn authorize_writer(
        &self,
        scope: &Scope,
        id: &str,
        incarnation: &str,
    ) -> Result<Assignment> {
        self.verify_member(id, incarnation)?;
        let assignment = self.assignment(scope)?;
        if assignment.placement.writer != id
            || assignment
                .placement
                .incarnations
                .get(id)
                .map(String::as_str)
                != Some(incarnation)
        {
            return Err(Error::Unavailable("writer assignment changed".into()));
        }
        Ok(assignment)
    }
    /// Remove a previous owner's key only while this exact assignment remains current.
    /// New Kafka producer activation performs the second, broker-side fence.
    pub fn retire_owner(&self, scope: &Scope, id: &str, incarnation: &str) -> Result<()> {
        let (revision, state) = self.load()?;
        let a = state
            .assignments
            .get(&scope.collection)
            .filter(|a| {
                a.scope == *scope
                    && a.placement.writer == id
                    && a.placement.incarnations.get(id).map(String::as_str) == Some(incarnation)
            })
            .ok_or_else(|| Error::Conflict("writer assignment replaced".into()))?;
        let _ = a;
        self.verify_member(id, incarnation)?;
        let catalog = Catalog::new(self.endpoint(), scope)?;
        if let Some((owner_revision, _)) = catalog.get("owner")? {
            let r = self.store.post("/v3/kv/txn", json!({"compare":[compare(&self.store,"state",&revision), self.incarnation_compare(id, incarnation), compare(&catalog,"owner",&owner_revision)],"success":[{"request_delete_range":{"key":catalog.key("owner")}}],"failure":[]}))?;
            if r["succeeded"] != true {
                return Err(Error::Conflict("owner retirement raced".into()));
            }
        }
        Ok(())
    }
    // Kept in Catalog so all clients retain one validated backend configuration.
    fn endpoint(&self) -> String {
        self.store.endpoint().to_string()
    }
    pub fn set_draining(&self, id: &str, draining: bool) -> Result<()> {
        crate::identifier(id)?;
        for _ in 0..16 {
            let (rev, mut s) = self.load()?;
            if draining {
                s.draining.insert(id.into());
            } else {
                s.draining.remove(id);
            }
            if s.draining.len() > MAX_NODES {
                return Err(Error::Unavailable("drain metadata quota".into()));
            }
            let r = self.store.post("/v3/kv/txn", json!({"compare":[compare(&self.store,"state",&rev)],"success":[self.state_put(&s)?],"failure":[]}))?;
            if r["succeeded"] == true {
                return Ok(());
            }
        }
        Err(Error::Unavailable("drain update raced".into()))
    }
    /// Publish warmed routing and completion in the same etcd transaction.
    pub fn publish(
        &self,
        lease: &Lease,
        revision: &str,
        state: &mut ControlState,
        directory: &Directory,
        collection: &str,
        members: &BTreeMap<String, Member>,
    ) -> Result<bool> {
        let (dir_revision, mut d) = directory.load()?;
        let task = state
            .assignments
            .get_mut(collection)
            .ok_or_else(|| Error::Conflict("assignment gone".into()))?;
        let c = d
            .collections
            .get_mut(collection)
            .filter(|c| c.scope == task.scope && c.state == Lifecycle::Active)
            .ok_or_else(|| Error::Conflict("collection not active".into()))?;
        if c.query_routes.as_ref().map_or(0, |r| r.epoch) + 1 != task.route_epoch
            || task.phase != Phase::Warming
            || task.warmed_offset.is_none()
        {
            return Err(Error::Conflict(
                "routing publication is stale/not warmed".into(),
            ));
        }
        let routes = QueryRoutes {
            epoch: task.route_epoch,
            hash_version: 1,
            shards: task
                .placement
                .replicas
                .iter()
                .map(|ids| {
                    ids.iter()
                        .map(|id| {
                            members.get(id).map(|m| m.origin.clone()).ok_or_else(|| {
                                Error::Unavailable("query member disappeared".into())
                            })
                        })
                        .collect()
                })
                .collect::<Result<Vec<_>>>()?,
        };
        routes.validate(c.shard_count)?;
        c.query_routes = Some(routes);
        task.phase = Phase::Active;
        task.error = None;
        task.attempts = 0;
        state.leader_epoch = lease.epoch;
        state.last_progress_ms = now_ms();
        state.reconciliations += 1;
        let mut compares = vec![
            self.leader_compare(lease)?,
            self.leader_member_compare(lease)?,
            compare(&self.store, "state", revision),
            directory.compare(&dir_revision),
        ];
        // Membership leases must still exist at publication. Heartbeat revisions may
        // change; incarnation equality was checked by every warm RPC.
        for (id, incarnation) in &state.assignments[collection].placement.incarnations {
            compares.push(self.incarnation_compare(id, incarnation));
        }
        let r = self.store.post("/v3/kv/txn",json!({"compare":compares,"success":[directory.put(&d)?,self.state_put(state)?],"failure":[]}))?;
        Ok(r["succeeded"] == true)
    }
}
/// Stable capacity-aware placement. One collection is migrated per reconciliation;
/// each shard's replicas occupy distinct failure domains whenever available.
pub fn plan(
    collections: &BTreeMap<String, Collection>,
    members: &BTreeMap<String, Member>,
    state: &ControlState,
) -> BTreeMap<String, Placement> {
    let mut writers = BTreeMap::<String, usize>::new();
    let mut shards = BTreeMap::<String, usize>::new();
    let mut query_collections = BTreeMap::<String, BTreeSet<String>>::new();
    let mut out = BTreeMap::new();
    for (id, c) in collections
        .iter()
        .filter(|(_, c)| c.state != Lifecycle::Dropped)
    {
        let candidates = members.values().filter(|m| !state.draining.contains(&m.id));
        let writer = candidates
            .clone()
            .filter(|m| {
                m.role == NodeRole::Writer
                    && writers.get(&m.id).copied().unwrap_or(0) < m.max_collections
            })
            .min_by_key(|m| {
                (
                    writers.get(&m.id).copied().unwrap_or(0),
                    crate::digest(format!("{id}/{}", m.id).as_bytes()),
                )
            });
        let Some(writer) = writer else { continue };
        let mut replicas = vec![];
        let mut local = shards.clone();
        for shard in 0..c.shard_count {
            let mut zones = BTreeSet::new();
            let mut ids = vec![];
            for _ in 0..state.replicas {
                let node = candidates
                    .clone()
                    .filter(|m| {
                        m.role == NodeRole::Query
                            && !zones.contains(&m.zone)
                            && local.get(&m.id).copied().unwrap_or(0) < m.max_shards
                            && query_collections
                                .get(&m.id)
                                .is_none_or(|set| set.contains(id) || set.len() < m.max_collections)
                    })
                    .min_by_key(|m| {
                        (
                            local.get(&m.id).copied().unwrap_or(0),
                            crate::digest(format!("{id}/{shard}/{}", m.id).as_bytes()),
                        )
                    });
                if let Some(m) = node {
                    query_collections
                        .entry(m.id.clone())
                        .or_default()
                        .insert(id.clone());
                    zones.insert(m.zone.clone());
                    ids.push(m.id.clone());
                    *local.entry(m.id.clone()).or_default() += 1;
                }
            }
            replicas.push(ids);
        }
        // Writer/lifecycle assignment can proceed before any Query node joins.
        *writers.entry(writer.id.clone()).or_default() += 1;
        shards = local;
        let mut incarnations = BTreeMap::from([(writer.id.clone(), writer.incarnation.clone())]);
        for id in replicas.iter().flatten() {
            incarnations.insert(id.clone(), members[id].incarnation.clone());
        }
        out.insert(
            id.clone(),
            Placement {
                writer: writer.id.clone(),
                replicas,
                incarnations,
            },
        );
    }
    out
}
