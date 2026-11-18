use super::*;
use std::collections::BTreeSet;
use vectordb_cluster::{
    control::{Assignment, Control, Member, NodeRole, PROTOCOL, Phase, plan},
    sharded::ShardQuery,
    transport::{self, wire::CallRequest},
};

pub(super) fn initialize(config: &mut Config) -> Result<Option<(Member, String)>> {
    let control = Control::new(config.etcd.clone(), &config.cluster)?;
    if !config.coordinated {
        if control.load()?.0 != "0" {
            return Err(Error::Conflict(
                "cluster has a coordinator; --coordinated and mTLS are required".into(),
            ));
        }
        return Ok(None);
    }
    let required = |v: &Option<String>| {
        v.clone().ok_or_else(|| {
            Error::Invalid(
                "coordinated nodes require RPC bind/origin, CA, certificate and key".into(),
            )
        })
    };
    if config.rpc_bind.is_none() {
        return Err(Error::Invalid("RPC bind required".into()));
    }
    let origin = advertised_origin(config.advertise.as_deref())?;
    config.advertise = Some(
        reqwest::Url::parse(&origin)
            .map_err(|_| Error::Invalid("invalid advertise origin".into()))?
            .origin()
            .ascii_serialization(),
    );
    let rpc_origin = advertised_origin(Some(&required(&config.rpc_advertise)?))?;
    config.rpc_advertise = Some(
        reqwest::Url::parse(&rpc_origin)
            .map_err(|_| Error::Invalid("invalid RPC origin".into()))?
            .origin()
            .ascii_serialization(),
    );
    config.incarnation = uuid::Uuid::new_v4().simple().to_string();
    let tls = transport::Tls::load(
        &required(&config.tls_ca)?,
        &required(&config.tls_cert)?,
        &required(&config.tls_key)?,
        &config.node_id,
    )?;
    let member = Member {
        id: config.node_id.clone(),
        incarnation: config.incarnation.clone(),
        role: match config.role {
            Role::Coordinator => NodeRole::Coordinator,
            Role::Node => NodeRole::Writer,
            Role::QueryNode => NodeRole::Query,
            _ => return Err(Error::Invalid("unsupported coordinated role".into())),
        },
        zone: config.zone.clone(),
        origin: advertised_origin(config.advertise.as_deref())?,
        rpc: required(&config.rpc_advertise)?,
        protocol_min: PROTOCOL,
        protocol_max: PROTOCOL,
        max_shards: config.max_shards,
        max_collections: MAX_COLLECTIONS,
        resources: json!({}),
    };
    member.validate()?;
    if config.role == Role::Coordinator {
        control.initialize(config.query_replicas, config.maintenance_ms)?;
    }
    let lease = control.register(&member)?;
    config.rpc = Some(transport::Client {
        control,
        cluster: config.cluster.clone(),
        sender: config.node_id.clone(),
        incarnation: config.incarnation.clone(),
        tls,
    });
    Ok(Some((member, lease)))
}
impl Node {
    pub(super) fn check_alive(&self) -> Result<()> {
        if !self.alive.load(Ordering::Acquire) {
            return Err(Error::Unavailable(
                "membership/server failed; restart node".into(),
            ));
        }
        if let Some(rpc) = &self.config.rpc {
            rpc.control
                .verify_member(&self.config.node_id, &self.config.incarnation)?;
        }
        Ok(())
    }
    pub(super) fn shutdown_membership(&self) {
        for session in self.sessions.lock().unwrap().values() {
            let runtime = session.lock().unwrap();
            runtime.failed.store(true, Ordering::Release);
            runtime.replica.stop_index_jobs();
            if let Some(lease) = &runtime.lease {
                let _ = runtime.catalog.revoke(&lease.id);
            }
        }
        if let Some((_, lease)) = &self.membership {
            let _ = self.config.rpc.as_ref().unwrap().control.revoke(lease);
        }
        if let Some(lease) = self.leader.lock().unwrap().take() {
            let _ = self.config.rpc.as_ref().unwrap().control.revoke(&lease.id);
        }
    }
    pub(super) fn readiness(&self) -> Result<()> {
        self.check_alive()?;
        if let Some(rpc) = &self.config.rpc {
            rpc.control.leader()?;
            let (_, s) = rpc.control.load()?;
            for a in s
                .assignments
                .values()
                .filter(|a| a.placement.writer == self.config.node_id)
            {
                let owner = Catalog::new(self.config.etcd.clone(), &a.scope)?.active_owner()?;
                if !owner.address.starts_with(&format!(
                    "{}/internal/",
                    self.config
                        .advertise
                        .as_deref()
                        .unwrap()
                        .trim_end_matches('/')
                )) {
                    return Err(Error::Unavailable("assigned writer is recovering".into()));
                }
            }
            if self.config.role == Role::QueryNode {
                let directory = self.directory.snapshot()?;
                let caches = self.shard_cache.lock().unwrap();
                for c in directory
                    .collections
                    .values()
                    .filter(|c| c.state == Lifecycle::Active)
                {
                    if let Some(r) = &c.query_routes
                        && !r
                            .assigned(
                                self.config
                                    .advertise
                                    .as_deref()
                                    .unwrap()
                                    .trim_end_matches('/'),
                            )
                            .is_empty()
                        && !caches.contains_key(&format!("{}/{}", c.scope.prefix(), r.epoch))
                    {
                        return Err(Error::Unavailable(
                            "assigned query shards are recovering".into(),
                        ));
                    }
                }
            }
        }
        Ok(())
    }
    fn resources(&self) -> Value {
        let mut writers = vec![];
        let mut replicas = vec![];
        let mut bytes = 0;
        if let Ok(sessions) = self.sessions.try_lock() {
            for (id, session) in sessions.iter() {
                if let Ok(runtime) = session.try_lock() {
                    bytes += runtime.replica.state_bytes();
                    writers.push(json!({"collection":id,"offset":runtime.replica.offset,"failed":runtime.failed.load(Ordering::Acquire),"stateBytes":runtime.replica.state_bytes()}));
                }
            }
        }
        if let Ok(caches) = self.shard_cache.try_lock() {
            for (key, cache) in caches.iter() {
                if let Ok(cache) = cache.try_lock()
                    && let Some(replica) = cache.as_ref()
                {
                    bytes += replica.bytes();
                    replicas.push(json!({"cache":key,"collection":replica.scope.collection,"offset":replica.offset,"shards":replica.engines.keys().collect::<Vec<_>>(),"rows":replica.rows(),"stateBytes":replica.bytes()}));
                }
            }
        }
        json!({"writers":writers,"replicas":replicas,"stateBytes":bytes,"observedAtMs":now_ms(),"backgroundError":*self.background_error.lock().unwrap(),"snapshotWaiters":self.snapshot_waiters.load(Ordering::Relaxed),"snapshotWaitMs":self.snapshot_wait_ms.load(Ordering::Relaxed),"snapshotWaitCount":self.snapshot_wait_count.load(Ordering::Relaxed)})
    }
    fn retire_sessions(&self) -> Result<()> {
        let rpc = self.config.rpc.as_ref().unwrap();
        let (_, s) = rpc.control.load()?;
        let mut sessions = self.sessions.lock().unwrap();
        sessions.retain(|id, session| {
            let runtime = session.lock().unwrap();
            let assigned = s.assignments.get(id).is_some_and(|a| {
                a.placement.writer == self.config.node_id
                    && a.placement.incarnations.get(&self.config.node_id)
                        == Some(&self.config.incarnation)
            });
            if !assigned || runtime.failed.load(Ordering::Acquire) {
                runtime.failed.store(true, Ordering::Release);
                if let Some(lease) = &runtime.lease {
                    let _ = runtime.catalog.revoke(&lease.id);
                }
                return false;
            }
            true
        });
        Ok(())
    }
    fn status_value(&self) -> Result<Value> {
        let rpc = self
            .config
            .rpc
            .as_ref()
            .ok_or_else(|| Error::Unsupported("no coordinator configured".into()))?;
        let (_, state) = rpc.control.load()?;
        let members = rpc.control.members()?;
        let degraded = state.assignments.values().any(|a| {
            a.phase != Phase::Active
                || a.placement.replicas.iter().any(|r| {
                    r.len() < state.replicas || r.iter().any(|id| !members.contains_key(id))
                })
                || !members.contains_key(&a.placement.writer)
        });
        Ok(
            json!({"code":0,"data":{"leader":rpc.control.leader().ok(),"state":state,"members":members,"degraded":degraded,"local":self.resources(),"protocol":PROTOCOL}}),
        )
    }
    fn warm(&self, assignment: &Assignment, pending: bool) -> Result<i64> {
        let rpc = self.config.rpc.as_ref().unwrap();
        let c = self.directory.snapshot()?.by_scope(&assignment.scope)?;
        let shards = assignment
            .placement
            .replicas
            .iter()
            .enumerate()
            .filter(|(_, ids)| ids.contains(&self.config.node_id))
            .map(|(s, _)| s)
            .collect::<BTreeSet<_>>();
        if shards.is_empty() {
            return Err(Error::Conflict("node has no target shards".into()));
        }
        let catalog = Catalog::new(self.config.etcd.clone(), &c.scope)?;
        let pinned = catalog.pin_latest()?;
        let (manifest, pin, lease) = match pinned {
            Some((m, p, l)) => (Some(m), Some(p), Some(l)),
            None => (None, None, None),
        };
        let result = (|| {
            let owner = catalog.active_owner()?;
            let response = rpc.call(
                &format!("{}/internal/snapshot", owner.address),
                json!({}),
                None,
                now_ms() + 30_000,
            )?;
            if response["code"] != 0 {
                return Err(Error::Unavailable("writer snapshot unavailable".into()));
            }
            let token: Token = serde_json::from_value(response["data"]["token"].clone())?;
            let offset = token.offset;
            self.shard_query(ShardQuery {
                token,
                route_epoch: assignment.route_epoch,
                shard_count: c.shard_count,
                shards,
                manifest,
                pin,
                operation: "entities/count".into(),
                requests: vec![json!({"collectionName":c.scope.collection})],
                deadline_ms: now_ms() + 30_000,
                warm_version: pending.then_some(assignment.version),
            })?;
            Ok(offset)
        })();
        if let Some(lease) = lease {
            let _ = catalog.revoke(&lease);
        }
        result
    }
    fn controller_step(&self, lease: &Lease) -> Result<()> {
        let rpc = self.config.rpc.as_ref().unwrap();
        let control = &rpc.control;
        let members = control.members()?;
        let directory = self.directory.snapshot()?;
        let (revision, mut state) = control.load()?;
        let desired = plan(&directory.collections, &members, &state);
        // Retire dropped identities and stale tasks before any further side effects.
        if state.assignments.keys().any(|id| {
            directory
                .collections
                .get(id)
                .is_none_or(|c| c.state == Lifecycle::Dropped)
        }) {
            state.assignments.retain(|id, _| {
                directory
                    .collections
                    .get(id)
                    .is_some_and(|c| c.state != Lifecycle::Dropped)
            });
            control.save(lease, &revision, &mut state)?;
            return Ok(());
        }
        for (id, placement) in desired {
            if state
                .assignments
                .get(&id)
                .is_none_or(|a| a.placement != placement)
            {
                let previous = state.assignments.get(&id);
                let version = previous.map_or(1, |a| a.version + 1);
                let c = &directory.collections[&id];
                state.assignments.insert(
                    id,
                    Assignment {
                        scope: c.scope.clone(),
                        version,
                        route_epoch: c.query_routes.as_ref().map_or(1, |r| r.epoch + 1),
                        placement,
                        phase: Phase::Pending,
                        attempts: 0,
                        retry_at_ms: 0,
                        error: None,
                        maintenance_sequence: previous.map_or(0, |a| a.maintenance_sequence),
                        maintenance_pending: false,
                        maintenance_completed_ms: previous
                            .map_or(0, |a| a.maintenance_completed_ms),
                        warmed_offset: None,
                    },
                );
                control.save(lease, &revision, &mut state)?;
                return Ok(());
            }
        }
        // Bounded: at most one collection's migration/maintenance per iteration.
        let mut candidates = state
            .assignments
            .values()
            .filter(|a| a.retry_at_ms <= now_ms())
            .cloned()
            .collect::<Vec<_>>();
        candidates.sort_by_key(|a| (a.phase == Phase::Active, a.maintenance_completed_ms));
        for assignment in candidates {
            let id = assignment.scope.collection.clone();
            let c = &directory.collections[&id];
            let owner = Catalog::new(self.config.etcd.clone(), &c.scope)?.active_owner();
            let writer = members.get(&assignment.placement.writer);
            let owner_ready = writer.is_some_and(|m| {
                owner
                    .as_ref()
                    .is_ok_and(|o| o.address.starts_with(&format!("{}/internal/", m.origin)))
            });
            let maintenance_due = assignment.maintenance_pending
                || now_ms() - assignment.maintenance_completed_ms >= state.maintenance_ms;
            if assignment.phase == Phase::Active
                && owner_ready
                && c.state == Lifecycle::Active
                && !maintenance_due
            {
                continue;
            }
            let operation = (|| -> Result<()> {
                let writer = writer.ok_or_else(|| {
                    Error::Unavailable("writer placement has no live member".into())
                })?;
                let payload = json!({"scope":assignment.scope,"version":assignment.version,"leaderEpoch":lease.epoch});
                let call = |member: &Member, path: &str, body: Value| -> Result<Value> {
                    let r = rpc.call(
                        &format!("{}{path}", member.origin),
                        body,
                        None,
                        now_ms() + 30_000,
                    )?;
                    if r["code"] != 0 {
                        return Err(Error::Unavailable(format!("task failed: {}", r["message"])));
                    }
                    Ok(r)
                };
                if !owner_ready
                    || c.state != Lifecycle::Active
                    || assignment.phase == Phase::Pending
                {
                    call(writer, "/control/ensure", payload.clone())?;
                }
                // Lifecycle may have completed on the writer; next loop refreshes catalog.
                if c.state != Lifecycle::Active {
                    return Ok(());
                }
                if assignment.phase == Phase::Pending {
                    state.assignments.get_mut(&id).unwrap().phase = Phase::Warming;
                    control.save(lease, &revision, &mut state)?;
                    return Ok(());
                }
                if assignment.phase == Phase::Warming {
                    if assignment.placement.replicas.iter().any(Vec::is_empty) {
                        return Err(Error::Unavailable(
                            "no query capacity in an available failure domain".into(),
                        ));
                    }
                    let mut minimum = i64::MAX;
                    for node in assignment
                        .placement
                        .replicas
                        .iter()
                        .flatten()
                        .collect::<BTreeSet<_>>()
                    {
                        let member = members
                            .get(node)
                            .ok_or_else(|| Error::Unavailable("warm target disappeared".into()))?;
                        let r = call(member, "/control/warm", payload.clone())?;
                        minimum = minimum.min(
                            r["data"]["offset"]
                                .as_i64()
                                .ok_or_else(|| Error::Integrity("warm offset absent".into()))?,
                        );
                    }
                    state.assignments.get_mut(&id).unwrap().warmed_offset = Some(minimum);
                    failpoint("routing_warmed");
                    control.publish(
                        lease,
                        &revision,
                        &mut state,
                        &self.directory,
                        &id,
                        &members,
                    )?;
                    failpoint("routing_published");
                    return Ok(());
                }
                if maintenance_due {
                    if !assignment.maintenance_pending {
                        let a = state.assignments.get_mut(&id).unwrap();
                        a.maintenance_pending = true;
                        a.maintenance_sequence += 1;
                        control.save(lease, &revision, &mut state)?;
                        return Ok(());
                    }
                    let mut payload = payload;
                    payload["sequence"] = json!(assignment.maintenance_sequence);
                    call(writer, "/control/maintain", payload)?;
                    failpoint("maintenance_applied");
                    let a = state.assignments.get_mut(&id).unwrap();
                    a.maintenance_pending = false;
                    a.maintenance_completed_ms = now_ms();
                    a.error = None;
                    a.attempts = 0;
                    control.save(lease, &revision, &mut state)?;
                }
                Ok(())
            })();
            if let Err(error) = operation {
                let (rev, mut latest) = control.load()?;
                if let Some(a) = latest.assignments.get_mut(&id)
                    && a.version == assignment.version
                {
                    a.attempts = a.attempts.saturating_add(1);
                    a.retry_at_ms = now_ms() + 250 * (1i64 << a.attempts.min(4));
                    a.error = Some(error.to_string().chars().take(512).collect());
                    control.save(lease, &rev, &mut latest)?;
                }
                return Err(error);
            }
            return Ok(());
        }
        Ok(())
    }
}
impl transport::Handler for Node {
    fn execute(&self, request: CallRequest, peer: Member) -> Result<Value> {
        self.check_alive()?;
        let body: Value = serde_json::from_slice(&request.body)?;
        if request.path.starts_with("/control/") {
            let rpc = self.config.rpc.as_ref().unwrap();
            rpc.control.verify_leader(
                &peer.id,
                &peer.incarnation,
                body["leaderEpoch"].as_i64().unwrap_or(0),
            )?;
            if peer.role != NodeRole::Coordinator {
                return Err(Error::Invalid("coordinator identity required".into()));
            }
            let scope: Scope = serde_json::from_value(body["scope"].clone())?;
            let a = rpc.control.assignment(&scope)?;
            if body["version"].as_u64() != Some(a.version)
                || a.placement.incarnations.get(&self.config.node_id)
                    != Some(&self.config.incarnation)
            {
                return Err(Error::Conflict("stale assignment task".into()));
            }
            match request.path.as_str() {
                "/control/ensure" => {
                    rpc.control.authorize_writer(
                        &scope,
                        &self.config.node_id,
                        &self.config.incarnation,
                    )?;
                    let c = self.directory.snapshot()?.by_scope(&scope)?;
                    self.session(&c)?;
                    self.reconcile(&c)?;
                    Ok(json!({"code":0,"data":{}}))
                }
                "/control/warm" => {
                    if a.phase != Phase::Warming {
                        return Err(Error::Conflict("task is not warming".into()));
                    }
                    Ok(json!({"code":0,"data":{"offset":self.warm(&a,true)?}}))
                }
                "/control/maintain" => {
                    if !a.maintenance_pending
                        || body["sequence"].as_u64() != Some(a.maintenance_sequence)
                    {
                        return Err(Error::Conflict("stale maintenance task".into()));
                    }
                    let c = self.directory.snapshot()?.by_scope(&scope)?;
                    let session = self.session(&c)?;
                    let mut runtime = session.lock().unwrap();
                    runtime.guard()?;
                    runtime.barrier()?;
                    let indexes = runtime.replica.build_indexes();
                    // Raw state is checkpointed even if an index exceeds its budget;
                    // the durable task remains pending and retries index construction.
                    let result = runtime.compact()?;
                    indexes?;
                    Ok(json!({"code":0,"data":result}))
                }
                _ => Err(Error::Unsupported("unknown controller operation".into())),
            }
        } else if request.path == "/internal/shard-query" {
            self.shard_query(serde_json::from_value(body)?)
        } else {
            let parts = request
                .path
                .trim_start_matches('/')
                .split('/')
                .collect::<Vec<_>>();
            if parts.len() < 6 || parts[..2] != ["internal", "collections"] {
                return Err(Error::Invalid("invalid internal RPC path".into()));
            }
            let scope = Scope {
                cluster: self.config.cluster.clone(),
                collection: parts[2].into(),
                generation: parts[3].into(),
            };
            scope.validate()?;
            let op = if parts.len() == 6 && parts[4] == "internal" {
                format!("internal/{}", parts[5])
            } else if parts.len() == 8 && parts[4..6] == ["v2", "vectordb"] {
                format!("{}/{}", parts[6], parts[7])
            } else {
                return Err(Error::Invalid("unknown internal RPC path".into()));
            };
            if peer.role == NodeRole::Query
                && (is_write(&op)
                    || matches!(
                        op.as_str(),
                        "collections/flush" | "collections/compact" | "internal/reconcile"
                    ))
            {
                return Err(Error::Unsupported(
                    "Query identity cannot mutate writer state".into(),
                ));
            }
            self.internal(
                &scope,
                &op,
                body,
                (!request.idempotency_key.is_empty()).then_some(request.idempotency_key.as_str()),
            )
        }
    }
}
pub(super) fn start(node: &Arc<Node>) {
    if node.membership.is_none() {
        return;
    }
    let weak = Arc::downgrade(node);
    std::thread::spawn(move || {
        loop {
            let Some(node) = weak.upgrade() else { break };
            if !node.alive.load(Ordering::Acquire) {
                break;
            }
            let (mut member, lease) = node.membership.as_ref().unwrap().clone();
            let rpc = node.config.rpc.as_ref().unwrap();
            member.resources = node.resources();
            if let Err(e) = rpc.control.heartbeat(&member, &lease) {
                *node.background_error.lock().unwrap() = Some(e.to_string());
                node.alive.store(false, Ordering::Release);
                node.shutdown_membership();
                break;
            }
            {
                let mut leader = node.leader.lock().unwrap();
                if let Some(lease) = leader.as_ref()
                    && rpc.control.renew(lease).is_err()
                {
                    let _ = rpc.control.revoke(&lease.id);
                    *leader = None;
                }
            }
            drop(node);
            std::thread::sleep(Duration::from_secs(2));
        }
    });
    let weak = Arc::downgrade(node);
    std::thread::spawn(move || {
        loop {
            let Some(node) = weak.upgrade() else { break };
            if !node.alive.load(Ordering::Acquire) {
                break;
            }
            let result = (|| -> Result<()> {
                let rpc = node.config.rpc.as_ref().unwrap();
                if node.config.role == Role::Coordinator {
                    let lease = {
                        let mut l = node.leader.lock().unwrap();
                        if l.is_none() {
                            *l = rpc
                                .control
                                .acquire(&node.config.node_id, &node.config.incarnation)
                                .ok();
                        }
                        l.clone()
                    };
                    if let Some(lease) = lease {
                        node.controller_step(&lease)?;
                    }
                } else if node.config.role == Role::Node {
                    node.retire_sessions()?;
                } else {
                    for a in rpc.control.load()?.1.assignments.values().filter(|a| {
                        a.phase == Phase::Active
                            && a.placement
                                .replicas
                                .iter()
                                .any(|r| r.contains(&node.config.node_id))
                    }) {
                        node.warm(a, false)?;
                    }
                }
                Ok(())
            })();
            *node.background_error.lock().unwrap() = result
                .err()
                .map(|e| e.to_string().chars().take(512).collect());
            drop(node);
            std::thread::sleep(Duration::from_millis(500));
        }
    });
}
pub(super) async fn status(State(app): State<NodeApp>, headers: HeaderMap) -> Response {
    run_node(app, headers, |n| n.status_value()).await
}
pub(super) async fn drain(
    State(app): State<NodeApp>,
    Path((id, action)): Path<(String, String)>,
    headers: HeaderMap,
) -> Response {
    run_node(app, headers, move |n| {
        let rpc = n
            .config
            .rpc
            .as_ref()
            .ok_or_else(|| Error::Unsupported("no coordinator".into()))?;
        if !["drain", "resume"].contains(&action.as_str()) {
            return Err(Error::Invalid("expected drain/resume".into()));
        }
        rpc.control.set_draining(&id, action == "drain")?;
        Ok(json!({"code":0,"data":{}}))
    })
    .await
}
pub(super) async fn metrics(State(app): State<NodeApp>, headers: HeaderMap) -> Response {
    if !authorized(&headers, &app.node.config.token) {
        return StatusCode::UNAUTHORIZED.into_response();
    }
    let permit = match app.slots.clone().try_acquire_owned() {
        Ok(permit) => permit,
        Err(_) => return StatusCode::SERVICE_UNAVAILABLE.into_response(),
    };
    match tokio::task::spawn_blocking(move || {
        let _permit = permit;
        app.node.status_value()
    })
    .await
    {
        Ok(Ok(s)) => {
            let state = &s["data"]["state"];
            let members = s["data"]["members"].as_object().map_or(0, |m| m.len());
            let tasks = state["assignments"]
                .as_object()
                .into_iter()
                .flat_map(|o| o.values())
                .collect::<Vec<_>>();
            let pending = tasks.iter().filter(|a| a["phase"] != "Active").count();
            let retries = tasks
                .iter()
                .map(|a| a["attempts"].as_u64().unwrap_or(0))
                .sum::<u64>();
            let progress = state["last_progress_ms"].as_i64().unwrap_or(0);
            let local = &s["data"]["local"];
            let mut writer_offsets = BTreeMap::<String, i64>::new();
            let mut replica_offsets = vec![];
            for member in s["data"]["members"]
                .as_object()
                .into_iter()
                .flat_map(|m| m.values())
            {
                for writer in member["resources"]["writers"]
                    .as_array()
                    .into_iter()
                    .flatten()
                {
                    if let (Some(id), Some(offset)) =
                        (writer["collection"].as_str(), writer["offset"].as_i64())
                    {
                        writer_offsets.insert(id.into(), offset);
                    }
                }
                for replica in member["resources"]["replicas"]
                    .as_array()
                    .into_iter()
                    .flatten()
                {
                    if let (Some(id), Some(offset)) =
                        (replica["collection"].as_str(), replica["offset"].as_i64())
                    {
                        replica_offsets.push((id.to_string(), offset));
                    }
                }
            }
            let lag = replica_offsets
                .iter()
                .map(|(id, o)| {
                    writer_offsets
                        .get(id)
                        .copied()
                        .unwrap_or(*o)
                        .saturating_sub(*o)
                        .max(0)
                })
                .max()
                .unwrap_or(0);
            let waiters = local["snapshotWaiters"].as_u64().unwrap_or(0);
            let wait_ms = local["snapshotWaitMs"].as_u64().unwrap_or(0);
            let wait_count = local["snapshotWaitCount"].as_u64().unwrap_or(0);
            ([(axum::http::header::CONTENT_TYPE,"text/plain; version=0.0.4")],format!("vectordb_live_nodes {members}\nvectordb_pending_assignments {pending}\nvectordb_task_attempts {retries}\nvectordb_last_progress_timestamp_seconds {}\nvectordb_degraded {}\nvectordb_query_wal_lag_offsets {lag}\nvectordb_snapshot_waiters {waiters}\nvectordb_snapshot_wait_ms_total {wait_ms}\nvectordb_snapshot_wait_count {wait_count}\n",progress as f64/1000.,u8::from(s["data"]["degraded"]==true))).into_response()
        }
        _ => StatusCode::SERVICE_UNAVAILABLE.into_response(),
    }
}
