//! Multi-collection gateway and distributed query routing.
mod controller;
mod sharding;
use super::*;
use std::collections::BTreeMap;
use vectordb_cluster::directory::{Collection, Directory, Lifecycle, MAX_COLLECTIONS, database};
use vectordb_cluster::state::{expiry, idempotency_key, now_ms};

// Available only in debug test binaries; no production request can enable it.
pub(super) fn injected(name: &str) -> bool {
    cfg!(debug_assertions) && std::env::var("VECTORDB_TEST_CRASH_AT").as_deref() == Ok(name)
}
pub(super) fn failpoint(name: &str) {
    if injected(name) {
        eprintln!("test crash at {name}");
        std::process::abort();
    }
}
struct Node {
    config: Config,
    directory: Directory,
    membership: Option<(vectordb_cluster::control::Member, String)>,
    alive: AtomicBool,
    leader: Mutex<Option<Lease>>,
    background_error: Mutex<Option<String>>,
    snapshot_waiters: std::sync::atomic::AtomicUsize,
    snapshot_wait_ms: std::sync::atomic::AtomicU64,
    snapshot_wait_count: std::sync::atomic::AtomicU64,

    sessions: Mutex<BTreeMap<String, Arc<Mutex<Runtime>>>>,
    shard_cache:
        Mutex<BTreeMap<String, Arc<Mutex<Option<vectordb_cluster::sharded::ShardReplica>>>>>,
}
#[derive(Clone)]
struct NodeApp {
    node: Arc<Node>,
    slots: Arc<tokio::sync::Semaphore>,
}
fn is_write(op: &str) -> bool {
    matches!(
        op,
        "databases/create"
            | "databases/drop"
            | "collections/create"
            | "collections/drop"
            | "collections/set_query_routes"
            | "aliases/create"
            | "aliases/alter"
            | "aliases/drop"
            | "partitions/create"
            | "partitions/drop"
            | "indexes/create"
            | "indexes/drop"
            | "entities/insert"
            | "entities/upsert"
            | "entities/delete"
    )
}
fn catalog_read(op: &str) -> bool {
    matches!(
        op,
        "databases/list"
            | "databases/describe"
            | "collections/list"
            | "collections/has"
            | "aliases/list"
            | "aliases/describe"
    )
}
const OPERATIONS: &[&str] = &[
    "databases/create",
    "databases/drop",
    "databases/list",
    "databases/describe",
    "collections/create",
    "collections/drop",
    "collections/list",
    "collections/has",
    "collections/describe",
    "collections/get_stats",
    "collections/get_load_state",
    "collections/flush",
    "aliases/create",
    "aliases/alter",
    "aliases/drop",
    "aliases/list",
    "aliases/describe",
    "partitions/create",
    "partitions/drop",
    "partitions/list",
    "partitions/has",
    "partitions/get_stats",
    "indexes/create",
    "indexes/drop",
    "indexes/list",
    "indexes/describe",
    "indexes/build",
    "indexes/retry",
    "indexes/cancel",
    "indexes/get_status",
    "entities/insert",
    "entities/upsert",
    "entities/delete",
    "entities/get",
    "entities/query",
    "entities/count",
    "entities/search",
    "entities/hybrid_search",
    "collections/set_query_routes",
    "collections/get_query_routes",
    "collections/shard_status",
];
fn supported(op: &str) -> bool {
    OPERATIONS.contains(&op)
}
fn available_operations(config: &Config) -> Vec<&'static str> {
    OPERATIONS
        .iter()
        .copied()
        .filter(|op| {
            if config.role == Role::Coordinator
                || (config.coordinated && *op == "collections/set_query_routes")
            {
                return false;
            }
            if config.role != Role::QueryNode {
                return true;
            }
            if config.coordinated {
                return !is_write(op) && *op != "collections/flush";
            }
            catalog_read(op)
                || matches!(
                    *op,
                    "entities/get"
                        | "entities/query"
                        | "entities/count"
                        | "entities/search"
                        | "entities/hybrid_search"
                        | "collections/get_query_routes"
                        | "collections/shard_status"
                )
        })
        .collect()
}
fn finish(work: Work) -> Result<Value> {
    match work {
        Work::Ready(value) => Ok(value),
        Work::Read {
            view,
            token,
            operation,
            request,
        } => Ok(json!({"code":0,"data":view.execute(&operation,request)?,"readToken":token})),
    }
}
impl Node {
    fn new(mut config: Config) -> Result<Self> {
        if config.token.len() < 16
            || config.advertise.is_none()
            || !(1..=5).contains(&config.wal_replicas)
        {
            return Err(Error::Invalid(
                "node requires token, advertise origin and WAL replicas 1..5".into(),
            ));
        }
        advertised_origin(config.advertise.as_deref())?;
        let directory = Directory::new(config.etcd.clone(), &config.cluster)?;
        directory.snapshot()?;
        let membership = controller::initialize(&mut config)?;
        Ok(Self {
            config,
            directory,
            membership,
            alive: AtomicBool::new(true),
            leader: Mutex::new(None),
            background_error: Mutex::new(None),
            snapshot_waiters: std::sync::atomic::AtomicUsize::new(0),
            snapshot_wait_ms: std::sync::atomic::AtomicU64::new(0),
            snapshot_wait_count: std::sync::atomic::AtomicU64::new(0),
            sessions: Mutex::new(BTreeMap::new()),
            shard_cache: Mutex::new(BTreeMap::new()),
        })
    }
    fn session(&self, c: &Collection) -> Result<Arc<Mutex<Runtime>>> {
        if matches!(self.config.role, Role::QueryNode | Role::Coordinator) {
            return Err(Error::Unsupported(
                "query-node cannot load full collection or own a writer".into(),
            ));
        }
        if let Some(rpc) = &self.config.rpc {
            rpc.control.authorize_writer(
                &c.scope,
                &self.config.node_id,
                &self.config.incarnation,
            )?;
        }
        let mut sessions = self.sessions.lock().unwrap();
        if let Some(session) = sessions.get(&c.scope.collection) {
            let usable = {
                let runtime = session.lock().unwrap();
                !runtime.failed.load(Ordering::Acquire)
                    && (runtime.lease.is_some() || runtime.catalog.owner().is_ok())
            };
            if usable {
                return Ok(session.clone());
            }
            // Retire ownership while the old queue is locked; outstanding Arc handles
            // may delay Drop, but may never delay fencing or reuse the old assignment.
            {
                let runtime = session.lock().unwrap();
                runtime.failed.store(true, Ordering::Release);
                runtime.replica.stop_index_jobs();
                if let Some(lease) = &runtime.lease {
                    runtime.catalog.revoke(&lease.id)?;
                }
            }
            sessions.remove(&c.scope.collection);
        }
        if sessions.len() >= MAX_COLLECTIONS {
            let state = self.directory.snapshot()?;
            sessions.retain(|id, _| {
                state
                    .collections
                    .get(id)
                    .is_some_and(|c| c.state != Lifecycle::Dropped)
            });
            if sessions.len() >= MAX_COLLECTIONS {
                return Err(Error::Unavailable(
                    "node collection capacity reached".into(),
                ));
            }
        }
        if let Some(rpc) = &self.config.rpc {
            rpc.control
                .retire_owner(&c.scope, &self.config.node_id, &self.config.incarnation)?;
        }
        let mut config = self.config.clone();
        config.collection = c.scope.collection.clone();
        config.generation = c.scope.generation.clone();
        config.managed = true;
        config.role = Role::Writer;
        let runtime = match Runtime::open(config.clone()) {
            Ok(runtime) => runtime,
            Err(Error::Conflict(_)) if !self.config.coordinated => {
                config.role = Role::Query;
                Runtime::open(config)?
            }
            Err(error) => return Err(error),
        };
        if runtime.lease.is_some() && c.state == Lifecycle::Active {
            self.directory.finish(
                &c.scope,
                Lifecycle::Active,
                runtime.replica.spec().cloned(),
                runtime.replica.offset,
            )?;
        }
        let runtime = Arc::new(Mutex::new(runtime));
        sessions.insert(c.scope.collection.clone(), runtime.clone());
        Ok(runtime)
    }
    fn reconcile(&self, c: &Collection) -> Result<()> {
        if !matches!(c.state, Lifecycle::Creating | Lifecycle::Dropping) {
            return Ok(());
        }
        let session = self.session(c)?;
        let mut runtime = session.lock().unwrap();
        if runtime.lease.is_none() {
            let value = runtime.forward("/internal/reconcile", json!({}), None)?;
            if value["code"] != 0 {
                return Err(Error::Unavailable(
                    "owner has not completed lifecycle transition".into(),
                ));
            }
            return Ok(());
        }
        runtime.guard()?;
        // Re-read after taking the per-collection queue; another gateway may have finished.
        let c = self.directory.snapshot()?.by_scope(&c.scope)?;
        let (op, id, body, next, point) = match c.state {
            Lifecycle::Creating => (
                "collections/create",
                c.create_id.as_str(),
                c.create_request.clone(),
                Lifecycle::Active,
                "create_committed",
            ),
            Lifecycle::Dropping => (
                "collections/drop",
                c.drop_id.as_deref().unwrap(),
                json!({"collectionName":c.scope.collection}),
                Lifecycle::Dropped,
                "drop_committed",
            ),
            _ => return Ok(()),
        };
        let needs_commit = if next == Lifecycle::Active {
            runtime.replica.spec().is_none()
        } else {
            runtime.replica.spec().is_some()
        };
        if needs_commit {
            // If an intent outlives the client retry window, first recovered state proves
            // the effect absent; only then issue a new internal control request.
            let recovery_id = idempotency_key(now_ms(), &uuid::Uuid::new_v4().simple().to_string());
            let id = if expiry(id, now_ms()).is_ok() {
                id
            } else {
                &recovery_id
            };
            finish(runtime.execute(op, body, Some(id))?)?;
        }
        failpoint(point);
        self.directory.finish(
            &c.scope,
            next,
            runtime.replica.spec().cloned(),
            runtime.replica.offset,
        )?;
        failpoint(if next == Lifecycle::Active {
            "catalog_active"
        } else {
            "catalog_dropped"
        });
        Ok(())
    }
    fn execute(&self, op: &str, mut r: Value, id: Option<&str>) -> Result<Value> {
        self.check_alive()?;
        if self.config.coordinated && op == "collections/set_query_routes" {
            return Err(Error::Unsupported("coordinator owns query routing".into()));
        }
        if self.config.role == Role::Coordinator {
            return Err(Error::Unsupported(
                "coordinator accepts cluster administration only".into(),
            ));
        }
        let timeout = r
            .as_object_mut()
            .and_then(|r| r.remove("timeoutMs"))
            .map(|v| {
                v.as_u64()
                    .filter(|n| *n > 0 && *n <= 30_000)
                    .ok_or_else(|| Error::Invalid("timeoutMs must be 1..30000".into()))
            })
            .transpose()?
            .unwrap_or(30_000);
        if self.config.role == Role::QueryNode && is_write(op) {
            return Err(Error::Unsupported("writes require a node gateway".into()));
        }
        if !supported(op) {
            return Err(Error::Unsupported(op.into()));
        }
        if catalog_read(op) {
            return Ok(json!({"code":0,"data":self.directory.snapshot()?.read(op,&r)?}));
        }
        if matches!(
            op,
            "collections/get_query_routes" | "collections/shard_status"
        ) && r.as_object().is_none_or(|obj| {
            obj.keys()
                .any(|k| !["dbName", "collectionName"].contains(&k.as_str()))
        }) {
            return Err(Error::Invalid("unknown query routing option".into()));
        }
        let db = database(&r)?.to_string();
        let target = if is_write(op) {
            let id = id.ok_or_else(|| Error::Invalid("Idempotency-Key header required".into()))?;
            let receipt = self.directory.begin(op, &r, id)?;
            if let Some(result) = receipt.result {
                return Ok(json!({"code":0,"data":result}));
            }
            receipt
                .target
                .ok_or_else(|| Error::Integrity("missing route binding".into()))?
        } else {
            self.directory
                .snapshot()?
                .resolve(
                    &db,
                    r["collectionName"]
                        .as_str()
                        .ok_or_else(|| Error::Invalid("collectionName required".into()))?,
                )?
                .scope
        };
        let c = self.directory.snapshot()?.by_scope(&target)?;
        if op == "collections/get_query_routes" {
            return Ok(
                json!({"code":0,"data":{"shardCount":c.shard_count,"hashVersion":1,"routes":c.query_routes}}),
            );
        }
        if op == "collections/shard_status" {
            return self.shard_stats(&c);
        }
        if op == "collections/create" || op == "collections/drop" {
            failpoint(if op == "collections/create" {
                "catalog_creating"
            } else {
                "catalog_dropping"
            });
            // A retry of an old create/drop must never operate on a replacement generation.
            if (op == "collections/create" && c.state == Lifecycle::Creating)
                || (op == "collections/drop" && c.state == Lifecycle::Dropping)
            {
                self.reconcile(&c)?;
            }
            return Ok(
                json!({"code":0,"data":{"collectionId":target.collection,"generation":target.generation}}),
            );
        }
        if is_write(op) {
            failpoint("route_bound");
        }
        let mut request = r;
        request["dbName"] = json!("default");
        request["collectionName"] = json!(target.collection);
        if matches!(
            op,
            "entities/search"
                | "entities/hybrid_search"
                | "entities/query"
                | "entities/get"
                | "entities/count"
        ) && c.query_routes.is_some()
        {
            return self.distributed(&c, op, request, timeout);
        }
        if self.config.coordinated && op.starts_with("entities/") && !is_write(op) {
            return Err(Error::Unavailable("query assignment is not ready".into()));
        }
        let mut value = self.internal(&target, op, request, id)?;
        if op == "collections/describe" && value["code"] == 0 {
            value["data"]["collectionName"] = json!(c.name);
            value["data"]["dbName"] = json!(db);
            value["data"]["collectionId"] = json!(target.collection);
            value["data"]["generation"] = json!(target.generation);
        }
        Ok(value)
    }
    fn internal(&self, scope: &Scope, op: &str, r: Value, id: Option<&str>) -> Result<Value> {
        let c = self.directory.snapshot()?.by_scope(scope)?;
        if let Some(rpc) = &self.config.rpc {
            self.check_alive()?;
            let assignment = rpc.control.assignment(scope)?;
            if assignment.placement.writer != self.config.node_id {
                let members = rpc.control.members()?;
                let writer = members
                    .get(&assignment.placement.writer)
                    .ok_or_else(|| Error::Unavailable("assigned writer unavailable".into()))?;
                let path = if op.starts_with("internal/") {
                    op.into()
                } else {
                    format!("v2/vectordb/{op}")
                };
                return rpc.call(
                    &format!(
                        "{}/internal/collections/{}/{}/{path}",
                        writer.origin, scope.collection, scope.generation
                    ),
                    r,
                    id,
                    now_ms() + 30_000,
                );
            }
        }
        if op == "internal/reconcile" {
            self.reconcile(&c)?;
            return Ok(json!({"code":0}));
        }
        if op == "collections/create" || op == "collections/drop" {
            return Err(Error::Invalid(
                "lifecycle mutations require catalog reconciliation".into(),
            ));
        }
        if !supported(op) && !["internal/barrier", "internal/snapshot"].contains(&op) {
            return Err(Error::Unsupported(op.into()));
        }
        if matches!(c.state, Lifecycle::Creating | Lifecycle::Dropping) {
            return Err(Error::Conflict("collection transition in progress".into()));
        }
        let session = self.session(&c)?;
        let (work, spec, offset) = {
            let mut runtime = session.lock().unwrap();
            // Internal requests may not redirect an obsolete owner to itself.
            if op == "internal/snapshot" {
                runtime.guard()?;
                let token = runtime.barrier()?;
                let spec = runtime
                    .replica
                    .spec()
                    .ok_or_else(|| Error::Conflict("collection dropped before snapshot".into()))?;
                return Ok(json!({"code":0,"data":{"token":token,"spec":spec}}));
            }
            if op == "internal/barrier" {
                runtime.guard()?;
                return Ok(json!({"code":0,"data":runtime.barrier()?}));
            }
            let work = runtime.execute(op, r, id)?;
            (
                work,
                runtime.replica.spec().cloned(),
                runtime.replica.offset,
            )
        };
        if matches!(
            op,
            "partitions/create" | "partitions/drop" | "indexes/create" | "indexes/drop"
        ) && c.state == Lifecycle::Active
        {
            self.directory
                .finish(scope, Lifecycle::Active, spec, offset)?;
        }
        finish(work)
    }
}
async fn run_node(
    app: NodeApp,
    headers: HeaderMap,
    action: impl FnOnce(&Node) -> Result<Value> + Send + 'static,
) -> Response {
    if !authorized(&headers, &app.node.config.token) {
        return (
            StatusCode::UNAUTHORIZED,
            Json(json!({"code":401,"message":"invalid token"})),
        )
            .into_response();
    }
    let permit = match app.slots.clone().try_acquire_owned() {
        Ok(p) => p,
        Err(_) => return error_response(Error::Unavailable("request queue full".into())),
    };
    match tokio::task::spawn_blocking(move || {
        let _permit = permit;
        action(&app.node)
    })
    .await
    {
        Ok(Ok(value)) => response(value),
        Ok(Err(error)) => error_response(error),
        Err(_) => error_response(Error::Unavailable("worker failed".into())),
    }
}
fn request_id(headers: &HeaderMap) -> Option<String> {
    headers
        .get("idempotency-key")
        .and_then(|v| v.to_str().ok())
        .map(str::to_owned)
}
async fn execute_node(
    State(app): State<NodeApp>,
    Path((category, action)): Path<(String, String)>,
    headers: HeaderMap,
    Json(body): Json<Value>,
) -> Response {
    let id = request_id(&headers);
    let timeout = body["timeoutMs"]
        .as_u64()
        .unwrap_or(30_000)
        .clamp(1, 30_000);
    match tokio::time::timeout(
        Duration::from_millis(timeout),
        run_node(app, headers, move |node| {
            node.execute(&format!("{category}/{action}"), body, id.as_deref())
        }),
    )
    .await
    {
        Ok(response) => response,
        Err(_) => error_response(Error::Unavailable("request deadline exceeded".into())),
    }
}
async fn internal_node(
    State(app): State<NodeApp>,
    Path((collection, generation, category, action)): Path<(String, String, String, String)>,
    headers: HeaderMap,
    Json(body): Json<Value>,
) -> Response {
    if app.node.config.coordinated {
        return error_response(Error::Unsupported(
            "internal operations require mTLS RPC".into(),
        ));
    }
    let id = request_id(&headers);
    run_node(app, headers, move |node| {
        node.internal(
            &Scope {
                cluster: node.config.cluster.clone(),
                collection,
                generation,
            },
            &format!("{category}/{action}"),
            body,
            id.as_deref(),
        )
    })
    .await
}
async fn control_node(
    State(app): State<NodeApp>,
    Path((collection, generation, action)): Path<(String, String, String)>,
    headers: HeaderMap,
    Json(body): Json<Value>,
) -> Response {
    if app.node.config.coordinated {
        return error_response(Error::Unsupported(
            "internal operations require mTLS RPC".into(),
        ));
    }
    run_node(app, headers, move |node| {
        node.internal(
            &Scope {
                cluster: node.config.cluster.clone(),
                collection,
                generation,
            },
            &format!("internal/{action}"),
            body,
            None,
        )
    })
    .await
}
async fn ready_node(State(app): State<NodeApp>, headers: HeaderMap) -> Response {
    run_node(app, headers, |node| {
        node.directory.snapshot()?;
        node.readiness()?;
        Ok(json!({"code":0,"ready":true}))
    })
    .await
}
pub(super) async fn serve(config: Config) -> std::result::Result<(), Box<dyn std::error::Error>> {
    let bind = config.bind;
    let node = Arc::new(tokio::task::spawn_blocking(move || Node::new(config)).await??);
    controller::start(&node);
    let weak = Arc::downgrade(&node);
    std::thread::spawn(move || {
        loop {
            let Some(node) = weak.upgrade() else { break };
            if let Ok(state) = node.directory.snapshot() {
                for c in state.collections.values() {
                    node.prune_shards(c);
                }
                for c in state.collections.values().filter(|c| {
                    node.config.role == Role::Node
                        && !node.config.coordinated
                        && matches!(c.state, Lifecycle::Creating | Lifecycle::Dropping)
                }) {
                    if let Err(e) = node.reconcile(c) {
                        eprintln!("lifecycle recovery pending: {e}");
                    }
                }
            }
            drop(node);
            std::thread::sleep(Duration::from_secs(1));
        }
    });
    let weak = Arc::downgrade(&node);
    std::thread::spawn(move || {
        loop {
            let Some(node) = weak.upgrade() else { break };
            let sessions = node
                .sessions
                .lock()
                .unwrap()
                .values()
                .cloned()
                .collect::<Vec<_>>();
            let shard_caches = node
                .shard_cache
                .lock()
                .unwrap()
                .values()
                .cloned()
                .collect::<Vec<_>>();
            drop(node);
            // A single scheduler bounds the node to two build workers, across collections.
            for session in sessions {
                let jobs = {
                    let runtime = session.lock().unwrap();
                    if runtime.failed.load(Ordering::Acquire) {
                        vec![]
                    } else {
                        runtime.replica.index_jobs()
                    }
                };
                let threads = jobs
                    .into_iter()
                    .map(|job| std::thread::spawn(move || job.run()))
                    .collect::<Vec<_>>();
                for thread in threads {
                    let _ = thread.join();
                }
            }
            for cache in shard_caches {
                let jobs = {
                    let cached = cache.lock().unwrap();
                    let mut jobs = vec![];
                    if let Some(replica) = cached.as_ref() {
                        for engine in replica.engines.values() {
                            if !jobs.is_empty() {
                                break;
                            }
                            jobs.extend(engine.index_jobs());
                        }
                    }
                    jobs
                };
                let threads = jobs
                    .into_iter()
                    .map(|job| std::thread::spawn(move || job.run()))
                    .collect::<Vec<_>>();
                for thread in threads {
                    let _ = thread.join();
                }
            }
            std::thread::sleep(Duration::from_millis(100));
        }
    });
    let state = NodeApp {
        node,
        slots: Arc::new(tokio::sync::Semaphore::new(32)),
    };
    let app=Router::new()
        .route("/healthz",get(||async{Json(json!({"status":"alive","mode":"cluster-m5"}))}))
        .route("/capabilities",get(|State(app): State<NodeApp>|async move{Json(json!({"mode":"cluster-m5","productionReady":false,"atomicScope":"collection","maxQueryShards":32,"shardedQueries":true,"consistency":["Strong"],"checkpointFormat":"state-segments-v4","persistentIndexes":true,"concurrentReads":true,"multiCollection":true,"namespaces":true,"aliases":true,"collectionLifecycle":true,"partitionDelete":true,"count":true,"automaticFailover":app.node.config.coordinated,"coordinated":app.node.config.coordinated,"internalProtocol":if app.node.config.coordinated {"grpc-mtls-v1"} else {"http-development"},"writerTakeover":if app.node.config.coordinated {"automatic"} else {"demand-driven recovery"},"idempotencyWindowMs":86400000,"maxActiveCollections":MAX_COLLECTIONS,"defaultVectorDtype":"Float32","role":match app.node.config.role {Role::Coordinator=>"coordinator",Role::QueryNode=>"query-node",_=>"node"},"clusterAdministration":app.node.config.coordinated,"operations":available_operations(&app.node.config),"maxRoutingReceipts":vectordb_cluster::directory::MAX_ROUTE_RECEIPTS}))}))
        .route("/readyz",get(ready_node))
        .route("/cluster/status",get(controller::status))
        .route("/cluster/nodes/{id}/{action}",post(controller::drain))
        .route("/metrics",get(controller::metrics))
        .route("/internal/shard-query",post(sharding::handle))
        .route("/v2/vectordb/{category}/{action}",post(execute_node))
        .route("/internal/collections/{collection}/{generation}/v2/vectordb/{category}/{action}",post(internal_node))
        .route("/internal/collections/{collection}/{generation}/internal/{action}",post(control_node))
        .layer(DefaultBodyLimit::max(512*1024)).with_state(state.clone());
    let (shutdown, receiver) = tokio::sync::watch::channel(false);
    let rpc_node = state.node.clone();
    let rpc_task = if let Some(client) = rpc_node.config.rpc.clone() {
        let bind = rpc_node.config.rpc_bind.unwrap();
        Some(tokio::spawn(async move {
            let mut receiver = receiver;
            let result =
                vectordb_cluster::transport::serve(bind, client, rpc_node.clone(), async move {
                    let _ = receiver.changed().await;
                })
                .await;
            if let Err(ref error) = result {
                rpc_node.alive.store(false, Ordering::SeqCst);
                eprintln!("RPC server failed: {error}");
            }
            result
        }))
    } else {
        None
    };
    let listener = tokio::net::TcpListener::bind(bind).await?;
    eprintln!("cluster HTTP listening on {}", listener.local_addr()?);
    axum::serve(listener, app)
        .with_graceful_shutdown(async {
            let ctrl = async {
                let _ = tokio::signal::ctrl_c().await;
            };
            #[cfg(unix)]
            let term = async {
                if let Ok(mut s) =
                    tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())
                {
                    s.recv().await;
                }
            };
            #[cfg(not(unix))]
            let term = std::future::pending::<()>();
            tokio::select! {_=ctrl=>{},_=term=>{}}
        })
        .await?;
    let _ = shutdown.send(true);
    if let Some(task) = rpc_task {
        task.await??;
    }
    state.node.alive.store(false, Ordering::SeqCst);
    tokio::task::spawn_blocking(move || {
        state.node.shutdown_membership();
        drop(state);
    })
    .await?;
    Ok(())
}
