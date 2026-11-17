use super::*;
use std::{
    collections::BTreeSet,
    io::Read,
    sync::atomic::{AtomicUsize, Ordering},
    time::Instant,
};
use vectordb_cluster::{
    sharded::{ShardPart, ShardQuery, ShardReplica, ShardReply},
    state::Checkpoint,
};
use vectordb_engine::distributed_query::QueryPlan;
const MAX_REPLY: usize = 32 * 1024 * 1024;
fn remaining(deadline: i64) -> Result<Duration> {
    let ms = deadline - now_ms();
    if ms <= 0 {
        return Err(Error::Unavailable("query deadline exceeded".into()));
    }
    Ok(Duration::from_millis(ms as u64))
}
struct Pin {
    catalog: Catalog,
    lease: Option<String>,
}
impl Drop for Pin {
    fn drop(&mut self) {
        if let Some(lease) = &self.lease {
            let _ = self.catalog.revoke(lease);
        }
    }
}
fn post(
    client: &reqwest::blocking::Client,
    address: &str,
    token: &str,
    body: &impl serde::Serialize,
    deadline: i64,
    budget: &AtomicUsize,
) -> Result<Value> {
    let mut response = client
        .post(address)
        .bearer_auth(token)
        .timeout(remaining(deadline)?)
        .json(body)
        .send()?;
    let mut bytes = vec![];
    let mut chunk = [0u8; 8192];
    loop {
        let n = response.read(&mut chunk)?;
        if n == 0 {
            break;
        }
        let mut used = budget.load(Ordering::Relaxed);
        loop {
            let total = used
                .checked_add(n)
                .filter(|v| *v <= MAX_REPLY)
                .ok_or_else(|| {
                    Error::Unavailable("aggregate query response exceeds budget".into())
                })?;
            match budget.compare_exchange_weak(used, total, Ordering::Relaxed, Ordering::Relaxed) {
                Ok(_) => break,
                Err(actual) => used = actual,
            }
        }
        bytes.extend_from_slice(&chunk[..n]);
    }
    let value: Value = serde_json::from_slice(&bytes)?;
    if value["code"] != 0 {
        return Err(Error::Unavailable(format!(
            "shard/owner request failed: {}",
            value["message"]
        )));
    }
    remaining(deadline)?;
    Ok(value)
}
impl Node {
    pub(super) fn shard_stats(&self, c: &Collection) -> Result<Value> {
        self.prune_shards(c);
        let cache = self.shard_cache.lock().unwrap();
        let mut rows = 0;
        let mut bytes = 0;
        let mut shards = vec![];
        let mut offsets = vec![];
        let mut records = 0;
        for (key, state) in cache
            .iter()
            .filter(|(key, _)| key.starts_with(&format!("{}/", c.scope.prefix())))
        {
            if let Some(state) = state.lock().unwrap().as_ref() {
                rows += state.rows();
                bytes += state.bytes();
                shards.extend(state.engines.keys().copied());
                offsets.push(json!({"cache":key,"offset":state.offset}));
                records += state.records;
            }
        }
        Ok(
            json!({"code":0,"data":{"rows":rows,"stateBytes":bytes,"shards":shards,"offsets":offsets,"walRecordsApplied":records,"fullCollectionSessions":self.sessions.lock().unwrap().len()}}),
        )
    }
    pub(super) fn prune_shards(&self, c: &Collection) {
        let prefix = format!("{}/", c.scope.prefix());
        let key = format!(
            "{}{}",
            prefix,
            c.query_routes.as_ref().map_or(0, |r| r.epoch)
        );
        self.shard_cache.lock().unwrap().retain(|k, _| {
            !k.starts_with(&prefix)
                || (*k == key
                    && c.state == Lifecycle::Active
                    && c.query_routes.as_ref().is_some_and(|r| {
                        !r.assigned(
                            self.config
                                .advertise
                                .as_deref()
                                .unwrap()
                                .trim_end_matches('/'),
                        )
                        .is_empty()
                    }))
        });
    }
    pub(super) fn distributed(
        &self,
        c: &Collection,
        op: &str,
        r: Value,
        timeout: u64,
    ) -> Result<Value> {
        let started = Instant::now();
        let deadline = now_ms() + timeout as i64;
        let client = reqwest::blocking::Client::builder()
            .redirect(reqwest::redirect::Policy::none())
            .build()?;
        let catalog = Catalog::new(self.config.etcd.clone(), &c.scope)?;
        let budget = AtomicUsize::new(0);
        let mut last = Error::Unavailable("no complete query snapshot".into());
        for _ in 0..3 {
            remaining(deadline)?;
            let c = self.directory.snapshot()?.by_scope(&c.scope)?;
            if c.state != Lifecycle::Active {
                return Err(Error::Conflict("collection is not active".into()));
            }
            let routes = c
                .query_routes
                .as_ref()
                .ok_or_else(|| Error::Conflict("query routes absent".into()))?;
            let pinned = catalog.pin_latest()?;
            let (manifest, pin, lease) = match pinned {
                Some((m, p, l)) => (Some(m), Some(p), Some(l)),
                None => (None, None, None),
            };
            let _pin = Pin {
                catalog: catalog.clone(),
                lease,
            };
            // Query-only nodes never construct a full collection runtime.
            if catalog.owner().is_err() && self.config.role == Role::Node {
                self.session(&c)?;
            }
            let owner = catalog.active_owner()?;
            let response = post(
                &client,
                &format!("{}/internal/snapshot", owner.address),
                &self.config.token,
                &json!({}),
                deadline,
                &budget,
            )?;
            let token: Token = serde_json::from_value(response["data"]["token"].clone())?;
            let spec: vectordb_engine::segment::CollectionSpec =
                serde_json::from_value(response["data"]["spec"].clone())?;
            if token.scope != c.scope
                || spec.shard_count != c.shard_count
                || manifest
                    .as_ref()
                    .is_some_and(|m| m.scope != token.scope || m.offset > token.offset)
            {
                return Err(Error::Integrity("snapshot identity/layout mismatch".into()));
            }
            let plan = QueryPlan::new(spec, op, r.clone())?;
            let template = ShardQuery {
                token: token.clone(),
                route_epoch: routes.epoch,
                shard_count: c.shard_count,
                shards: BTreeSet::new(),
                manifest,
                pin,
                operation: plan.operation.clone(),
                requests: plan.requests.clone(),
                deadline_ms: deadline,
            };
            let mut received = BTreeMap::<usize, ShardPart>::new();
            let mut errors = vec![];
            for replica in 0..3 {
                let mut groups = BTreeMap::<String, BTreeSet<usize>>::new();
                for (shard, replicas) in routes.shards.iter().enumerate() {
                    if !received.contains_key(&shard)
                        && let Some(address) = replicas.get(replica)
                    {
                        groups.entry(address.clone()).or_default().insert(shard);
                    }
                }
                if groups.is_empty() {
                    break;
                }
                let results = std::thread::scope(|scope| {
                    let handles = groups
                        .into_iter()
                        .map(|(address, shards)| {
                            let mut request = template.clone();
                            request.shards = shards;
                            let client = &client;
                            let budget = &budget;
                            scope.spawn(move || -> Result<ShardReply> {
                                let value = post(
                                    client,
                                    &format!("{address}/internal/shard-query"),
                                    &self.config.token,
                                    &request,
                                    now_ms() + (deadline - now_ms()).max(1) / (3 - replica) as i64,
                                    budget,
                                )?;
                                let reply: ShardReply =
                                    serde_json::from_value(value["data"].clone())?;
                                reply.validate(&request)?;
                                Ok(reply)
                            })
                        })
                        .collect::<Vec<_>>();
                    handles
                        .into_iter()
                        .map(|h| {
                            h.join().unwrap_or_else(|_| {
                                Err(Error::Unavailable("shard RPC worker failed".into()))
                            })
                        })
                        .collect::<Vec<_>>()
                });
                for result in results {
                    match result {
                        Ok(reply) => {
                            for part in reply.parts {
                                if received.insert(part.shard, part).is_some() {
                                    return Err(Error::Integrity(
                                        "duplicate shard response".into(),
                                    ));
                                }
                            }
                        }
                        Err(error) => errors.push(error),
                    }
                }
                if received.len() == c.shard_count {
                    break;
                }
            }
            if received.len() == c.shard_count {
                if self
                    .directory
                    .snapshot()?
                    .by_scope(&c.scope)?
                    .query_routes
                    .as_ref()
                    .map(|r| r.epoch)
                    != Some(routes.epoch)
                {
                    last = Error::Conflict("query routes changed during snapshot".into());
                    continue;
                }
                remaining(deadline)?;
                let data = plan.merge(received.into_values().map(|p| p.values).collect())?;
                return Ok(
                    json!({"code":0,"data":data,"readToken":token,"routeEpoch":routes.epoch,"queryMillis":started.elapsed().as_millis()}),
                );
            }
            last = errors
                .pop()
                .unwrap_or_else(|| Error::Unavailable("missing query shards".into()));
        }
        Err(last)
    }
    pub(super) fn shard_query(&self, request: ShardQuery) -> Result<Value> {
        remaining(request.deadline_ms)?;
        if request.deadline_ms > now_ms() + 30_000
            || request.requests.is_empty()
            || request.requests.len() > 16
            || !matches!(
                request.operation.as_str(),
                "entities/search" | "entities/query" | "entities/get" | "entities/count"
            )
        {
            return Err(Error::Invalid("invalid shard query envelope".into()));
        }
        let c = self.directory.snapshot()?.by_scope(&request.token.scope)?;
        let routes = c
            .query_routes
            .as_ref()
            .ok_or_else(|| Error::Conflict("query routing absent".into()))?;
        let address = self
            .config
            .advertise
            .as_deref()
            .unwrap()
            .trim_end_matches('/');
        let assigned = routes.assigned(address);
        if c.state != Lifecycle::Active
            || routes.epoch != request.route_epoch
            || c.shard_count != request.shard_count
            || request.shards.is_empty()
            || !request.shards.is_subset(&assigned)
        {
            return Err(Error::Conflict("stale route or unassigned shard".into()));
        }
        self.prune_shards(&c);
        let cache_key = format!("{}/{}", c.scope.prefix(), routes.epoch);
        let cache = {
            let mut cache = self.shard_cache.lock().unwrap();
            if cache.len() >= MAX_COLLECTIONS && !cache.contains_key(&cache_key) {
                return Err(Error::Unavailable("shard cache collection quota".into()));
            }
            cache
                .entry(cache_key)
                .or_insert_with(|| Arc::new(Mutex::new(None)))
                .clone()
        };
        let catalog = Catalog::new(self.config.etcd.clone(), &c.scope)?;
        let views = {
            let mut cached = cache.lock().unwrap();
            let use_cached = cached.as_ref().is_some_and(|r| {
                r.offset <= request.token.offset
                    && request
                        .manifest
                        .as_ref()
                        .is_none_or(|m| m.offset <= r.offset)
                    && r.engines.keys().copied().collect::<BTreeSet<_>>() == assigned
            });
            let mut replica = if use_cached {
                cached.take().unwrap()
            } else {
                self.load_shards(&catalog, &request, &assigned)?
            };
            let log = Log::reader(self.config.kafka.clone(), &c.scope);
            log.replay(replica.offset, request.token.offset, |offset, record| {
                remaining(request.deadline_ms)?;
                replica.apply(offset, record)
            })?;
            if replica.bytes() > 256 * 1024 * 1024 {
                return Err(Error::Unavailable(
                    "assigned query state quota exceeded".into(),
                ));
            }
            let views = replica.views(&request.token, &request.shards)?;
            if cached
                .as_ref()
                .is_none_or(|old| old.offset <= replica.offset)
            {
                *cached = Some(replica);
            }
            views
        };
        if cfg!(debug_assertions)
            && let Ok(delay) = std::env::var("VECTORDB_TEST_SHARD_DELAY_MS")
        {
            std::thread::sleep(Duration::from_millis(
                delay.parse::<u64>().unwrap_or(0).min(2000),
            ));
        }
        let mut parts = vec![];
        let mut output_bytes = 0;
        for (shard, view) in views {
            remaining(request.deadline_ms)?;
            let values = request
                .requests
                .iter()
                .map(|r| {
                    view.execute(&request.operation, r.clone())
                        .map_err(Error::from)
                })
                .collect::<Result<Vec<_>>>()?;
            output_bytes += serde_json::to_vec(&values)?.len();
            if output_bytes > MAX_REPLY {
                return Err(Error::Unavailable("shard output budget exceeded".into()));
            }
            parts.push(ShardPart { shard, values });
        }
        remaining(request.deadline_ms)?;
        let reply = ShardReply {
            token: request.token,
            route_epoch: request.route_epoch,
            parts,
        };
        if serde_json::to_vec(&reply)?.len() > MAX_REPLY {
            return Err(Error::Unavailable("shard output budget exceeded".into()));
        }
        Ok(json!({"code":0,"data":reply}))
    }
    fn load_shards(
        &self,
        catalog: &Catalog,
        request: &ShardQuery,
        assigned: &BTreeSet<usize>,
    ) -> Result<ShardReplica> {
        let Some(manifest) = &request.manifest else {
            return ShardReplica::empty(request.token.scope.clone(), request.shard_count, assigned);
        };
        let pin = request
            .pin
            .as_ref()
            .ok_or_else(|| Error::Invalid("snapshot pin required".into()))?;
        if manifest.scope != request.token.scope || manifest.offset > request.token.offset {
            return Err(Error::Integrity("foreign or newer checkpoint".into()));
        }
        catalog.verify_pin(pin)?;
        let objects = Objects::new(
            self.config.s3_endpoint.clone(),
            self.config.bucket.clone(),
            self.config.region.clone(),
            self.config.access_key.clone(),
            self.config.secret_key.clone(),
        )?;
        let bytes = objects.get(&manifest.key)?;
        if bytes.len() != manifest.bytes || digest(&bytes) != manifest.sha256 {
            return Err(Error::Integrity("checkpoint checksum mismatch".into()));
        }
        let checkpoint: Checkpoint = serde_json::from_slice(&bytes)?;
        if checkpoint.offset != manifest.offset {
            return Err(Error::Integrity("checkpoint offset mismatch".into()));
        }
        let prefix = manifest
            .key
            .rsplit_once('/')
            .ok_or_else(|| Error::Integrity("invalid checkpoint key".into()))?
            .0;
        let replica = ShardReplica::restore(
            checkpoint,
            &request.token.scope,
            request.shard_count,
            assigned,
            |key| {
                if key.contains('/') || !manifest.objects.contains(&format!("{prefix}/{key}")) {
                    return Err(vectordb_engine::Error::Integrity(
                        "object outside manifest".into(),
                    ));
                }
                remaining(request.deadline_ms).map_err(|e| {
                    vectordb_engine::Error::Io(std::io::Error::other(e.to_string()))
                })?;
                objects
                    .get(&format!("{prefix}/{key}"))
                    .map_err(|e| vectordb_engine::Error::Io(std::io::Error::other(e.to_string())))
            },
        )?;
        catalog.verify_pin(pin)?;
        remaining(request.deadline_ms)?;
        Ok(replica)
    }
}
pub(super) async fn handle(
    State(app): State<NodeApp>,
    headers: HeaderMap,
    Json(request): Json<ShardQuery>,
) -> Response {
    let timeout = request
        .deadline_ms
        .saturating_sub(now_ms())
        .clamp(1, 30_000) as u64;
    match tokio::time::timeout(
        Duration::from_millis(timeout),
        run_node(app, headers, move |node| node.shard_query(request)),
    )
    .await
    {
        Ok(response) => response,
        Err(_) => error_response(Error::Unavailable("shard deadline exceeded".into())),
    }
}
