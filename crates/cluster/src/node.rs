//! M3 multi-collection gateway. Assignment is demand-driven; M5 adds placement.
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
    sessions: Mutex<BTreeMap<String, Arc<Mutex<Runtime>>>>,
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
];
fn supported(op: &str) -> bool {
    OPERATIONS.contains(&op)
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
    fn new(config: Config) -> Result<Self> {
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
        Ok(Self {
            config,
            directory,
            sessions: Mutex::new(BTreeMap::new()),
        })
    }
    fn session(&self, c: &Collection) -> Result<Arc<Mutex<Runtime>>> {
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
        let mut config = self.config.clone();
        config.collection = c.scope.collection.clone();
        config.generation = c.scope.generation.clone();
        config.managed = true;
        config.role = Role::Writer;
        let runtime = match Runtime::open(config.clone()) {
            Ok(runtime) => runtime,
            Err(Error::Conflict(_)) => {
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
    fn execute(&self, op: &str, r: Value, id: Option<&str>) -> Result<Value> {
        if !supported(op) {
            return Err(Error::Unsupported(op.into()));
        }
        if catalog_read(op) {
            return Ok(json!({"code":0,"data":self.directory.snapshot()?.read(op,&r)?}));
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
        if op == "internal/reconcile" {
            self.reconcile(&c)?;
            return Ok(json!({"code":0}));
        }
        if op == "collections/create" || op == "collections/drop" {
            return Err(Error::Invalid(
                "lifecycle mutations require catalog reconciliation".into(),
            ));
        }
        if !supported(op) && op != "internal/barrier" {
            return Err(Error::Unsupported(op.into()));
        }
        if matches!(c.state, Lifecycle::Creating | Lifecycle::Dropping) {
            return Err(Error::Conflict("collection transition in progress".into()));
        }
        let session = self.session(&c)?;
        let (work, spec, offset) = {
            let mut runtime = session.lock().unwrap();
            // Internal requests may not redirect an obsolete owner to itself.
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
    run_node(app, headers, move |node| {
        node.execute(&format!("{category}/{action}"), body, id.as_deref())
    })
    .await
}
async fn internal_node(
    State(app): State<NodeApp>,
    Path((collection, generation, category, action)): Path<(String, String, String, String)>,
    headers: HeaderMap,
    Json(body): Json<Value>,
) -> Response {
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
        Ok(json!({"code":0,"ready":true}))
    })
    .await
}
pub(super) async fn serve(config: Config) -> std::result::Result<(), Box<dyn std::error::Error>> {
    let bind = config.bind;
    let node = Arc::new(tokio::task::spawn_blocking(move || Node::new(config)).await??);
    let weak = Arc::downgrade(&node);
    std::thread::spawn(move || {
        loop {
            let Some(node) = weak.upgrade() else { break };
            if let Ok(state) = node.directory.snapshot() {
                for c in state
                    .collections
                    .values()
                    .filter(|c| matches!(c.state, Lifecycle::Creating | Lifecycle::Dropping))
                {
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
            std::thread::sleep(Duration::from_millis(100));
        }
    });
    let state = NodeApp {
        node,
        slots: Arc::new(tokio::sync::Semaphore::new(32)),
    };
    let app=Router::new()
        .route("/healthz",get(||async{Json(json!({"status":"alive","mode":"cluster-m3"}))}))
        .route("/capabilities",get(||async{Json(json!({"mode":"cluster-m3","productionReady":false,"atomicScope":"collection","queryShards":1,"consistency":["Strong"],"checkpointFormat":"state-segments-v3","persistentIndexes":true,"concurrentReads":true,"multiCollection":true,"namespaces":true,"aliases":true,"collectionLifecycle":true,"partitionDelete":true,"count":true,"automaticFailover":false,"writerTakeover":"demand-driven recovery","idempotencyWindowMs":86400000,"maxActiveCollections":MAX_COLLECTIONS,"defaultVectorDtype":"Float32","operations":OPERATIONS,"maxRoutingReceipts":vectordb_cluster::directory::MAX_ROUTE_RECEIPTS}))}))
        .route("/readyz",get(ready_node))
        .route("/v2/vectordb/{category}/{action}",post(execute_node))
        .route("/internal/collections/{collection}/{generation}/v2/vectordb/{category}/{action}",post(internal_node))
        .route("/internal/collections/{collection}/{generation}/internal/{action}",post(control_node))
        .layer(DefaultBodyLimit::max(512*1024)).with_state(state.clone());
    let listener = tokio::net::TcpListener::bind(bind).await?;
    eprintln!("M3 node listening on {}", listener.local_addr()?);
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
    tokio::task::spawn_blocking(move || drop(state)).await?;
    Ok(())
}
