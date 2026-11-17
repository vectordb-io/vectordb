use axum::{
    Json, Router,
    extract::{DefaultBodyLimit, Path, State},
    http::{HeaderMap, StatusCode},
    response::{IntoResponse, Response},
    routing::{get, post},
};
use clap::{Parser, ValueEnum};
use serde_json::{Value, json};
use std::{
    net::SocketAddr,
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, Ordering},
    },
    time::Duration,
};
use vectordb_cluster::{
    Error, Result,
    catalog::{Catalog, Lease, Manifest},
    digest,
    kafka::Log,
    object::Objects,
    state::{Entry, FORMAT, Record, Replica, Scope, Token},
};

#[derive(Clone, Debug, ValueEnum, PartialEq)]
enum Role {
    Writer,
    Query,
    Checkpoint,
}
#[derive(Parser, Clone)]
#[command(about = "Experimental M1 collection-scoped distributed VectorDB")]
struct Config {
    #[arg(long, value_enum)]
    role: Role,
    #[arg(long, env = "VECTORDB_CLUSTER", default_value = "development")]
    cluster: String,
    #[arg(long, env = "VECTORDB_COLLECTION")]
    collection: String,
    #[arg(long, env = "VECTORDB_GENERATION")]
    generation: String,
    #[arg(long, env = "VECTORDB_BIND", default_value = "127.0.0.1:19531")]
    bind: SocketAddr,
    #[arg(long, env = "VECTORDB_ADVERTISE")]
    advertise: Option<String>,
    #[arg(long, env = "VECTORDB_ETCD", default_value = "http://127.0.0.1:2379")]
    etcd: String,
    #[arg(long, env = "VECTORDB_KAFKA", default_value = "127.0.0.1:9092")]
    kafka: String,
    #[arg(
        long,
        env = "VECTORDB_S3_ENDPOINT",
        default_value = "http://127.0.0.1:9000"
    )]
    s3_endpoint: String,
    #[arg(long, env = "VECTORDB_S3_BUCKET", default_value = "vectordb")]
    bucket: String,
    #[arg(long, env = "VECTORDB_S3_REGION", default_value = "us-east-1")]
    region: String,
    #[arg(long, env = "VECTORDB_S3_ACCESS_KEY", hide_env_values = true)]
    access_key: String,
    #[arg(long, env = "VECTORDB_S3_SECRET_KEY", hide_env_values = true)]
    secret_key: String,
    #[arg(long, env = "VECTORDB_CLUSTER_TOKEN", hide_env_values = true)]
    token: String,
}
enum Work {
    Ready(Value),
    Read {
        view: Box<vectordb_engine::query_index::ReadView>,
        token: Token,
        operation: String,
        request: Value,
    },
}
struct Runtime {
    config: Config,
    catalog: Catalog,
    objects: Objects,
    log: Log,
    replica: Replica,
    lease: Option<Lease>,
    failed: Arc<AtomicBool>,
    client: reqwest::blocking::Client,
    uncheckpointed_records: usize,
}
impl Runtime {
    fn open(config: Config) -> Result<Self> {
        if config.token.len() < 16 {
            return Err(Error::Invalid(
                "cluster token must be at least 16 characters".into(),
            ));
        }
        let scope = Scope {
            cluster: config.cluster.clone(),
            collection: config.collection.clone(),
            generation: config.generation.clone(),
        };
        scope.validate()?;
        let catalog = Catalog::new(config.etcd.clone(), &scope)?;
        let objects = Objects::new(
            config.s3_endpoint.clone(),
            config.bucket.clone(),
            config.region.clone(),
            config.access_key.clone(),
            config.secret_key.clone(),
        )?;
        let replica = Self::load_checkpoint(&catalog, &objects, &scope)?
            .unwrap_or(Replica::empty(scope.clone())?);
        let mut runtime = Self {
            config,
            uncheckpointed_records: 0,
            catalog,
            objects,
            log: Log::reader(String::new(), &scope),
            replica,
            lease: None,
            failed: Arc::new(AtomicBool::new(false)),
            client: reqwest::blocking::Client::builder()
                .timeout(Duration::from_secs(45))
                .redirect(reqwest::redirect::Policy::none())
                .build()?,
        };
        runtime.log = Log::reader(runtime.config.kafka.clone(), &scope);
        if runtime.config.role == Role::Writer {
            let address = runtime.config.advertise.clone().ok_or_else(|| {
                Error::Invalid(
                    "writer requires --advertise URL reachable by query processes".into(),
                )
            })?;
            let url = reqwest::Url::parse(&address)
                .map_err(|_| Error::Invalid("invalid advertise URL".into()))?;
            if !matches!(url.scheme(), "http" | "https")
                || !url.username().is_empty()
                || url.password().is_some()
                || url.query().is_some()
                || url.fragment().is_some()
                || url.path() != "/"
            {
                return Err(Error::Invalid("advertise must be an HTTP(S) origin".into()));
            }
            let lease = runtime
                .catalog
                .acquire(address.trim_end_matches('/').into())?;
            runtime.lease = Some(lease.clone());
            let catalog = runtime.catalog.clone();
            let failed = runtime.failed.clone();
            std::thread::spawn(move || {
                while !failed.load(Ordering::SeqCst) {
                    std::thread::sleep(Duration::from_secs(5));
                    if catalog.renew(&lease).is_err() {
                        failed.store(true, Ordering::SeqCst);
                        break;
                    }
                }
            });
            runtime.guard()?;
            runtime.log.initialize_writer()?;
            runtime.guard()?;
            let epoch = runtime.lease.as_ref().unwrap().epoch;
            let record = Record {
                format: FORMAT,
                scope,
                entry: Entry::Activate { epoch },
            };
            runtime.replica.check_capacity(&record)?;
            let offset = runtime.log.append(&record)?;
            runtime.catch_up(offset)?;
            runtime.guard()?;
        }
        Ok(runtime)
    }
    fn guard(&self) -> Result<()> {
        if self.failed.load(Ordering::SeqCst) {
            return Err(Error::Unavailable(
                "writer fenced or outcome unknown; restart after recovery".into(),
            ));
        }
        if let Some(lease) = &self.lease
            && let Err(error) = self.catalog.verify(lease)
        {
            self.failed.store(true, Ordering::SeqCst);
            return Err(error);
        }
        Ok(())
    }
    fn load_checkpoint(
        catalog: &Catalog,
        objects: &Objects,
        scope: &Scope,
    ) -> Result<Option<Replica>> {
        let Some((manifest, pin, lease)) = catalog.pin_latest()? else {
            return Ok(None);
        };
        let result = (|| {
            if manifest.scope != *scope {
                return Err(Error::Integrity("foreign manifest".into()));
            }
            let bytes = objects.get(&manifest.key)?;
            if bytes.len() != manifest.bytes || digest(&bytes) != manifest.sha256 {
                return Err(Error::Integrity("checkpoint checksum mismatch".into()));
            }
            let checkpoint: vectordb_cluster::state::Checkpoint = serde_json::from_slice(&bytes)?;
            let prefix = manifest
                .key
                .rsplit_once('/')
                .ok_or_else(|| Error::Integrity("invalid manifest path".into()))?
                .0;
            let replica = Replica::restore(checkpoint, scope, |key| {
                if key.contains('/') || !manifest.objects.contains(&format!("{prefix}/{key}")) {
                    return Err(vectordb_engine::Error::Invalid(
                        "object not in published manifest".into(),
                    ));
                }
                objects
                    .get(&format!("{prefix}/{key}"))
                    .map_err(|e| vectordb_engine::Error::Io(std::io::Error::other(e.to_string())))
            })?;
            catalog.verify_pin(&pin)?;
            if replica.offset != manifest.offset {
                return Err(Error::Integrity("manifest offset mismatch".into()));
            }
            Ok(Some(replica))
        })();
        let _ = catalog.revoke(&lease);
        result
    }
    fn catch_up(&mut self, offset: i64) -> Result<()> {
        let result = self
            .log
            .replay(self.replica.offset, offset, |position, record| {
                self.replica.apply(position, record)
            });
        if matches!(result, Err(Error::CheckpointRequired)) {
            self.replica =
                Self::load_checkpoint(&self.catalog, &self.objects, &self.replica.scope)?
                    .ok_or(Error::CheckpointRequired)?;
            return self
                .log
                .replay(self.replica.offset, offset, |position, record| {
                    self.replica.apply(position, record)
                });
        }
        result
    }
    fn commit(&mut self, record: Record) -> Result<i64> {
        self.guard()?;
        self.replica.check_capacity(&record)?;
        let result = (|| {
            let offset = self.log.append(&record)?;
            self.replica.apply(offset, record)?;
            self.guard()?;
            self.uncheckpointed_records += 1;
            if self.uncheckpointed_records >= 128 {
                self.flush_state()?;
            }
            Ok(offset)
        })();
        if result.is_err() {
            self.failed.store(true, Ordering::SeqCst);
        }
        result
    }
    fn barrier(&mut self) -> Result<Token> {
        if self.lease.is_some() {
            let epoch = self.replica.epoch;
            let offset = self.commit(Record {
                format: FORMAT,
                scope: self.replica.scope.clone(),
                entry: Entry::Barrier { epoch },
            })?;
            return Ok(Token {
                scope: self.replica.scope.clone(),
                offset,
            });
        }
        let response = self.forward("/internal/barrier", json!({}), None)?;
        let token: Token = serde_json::from_value(response["data"].clone())?;
        if token.scope != self.replica.scope {
            return Err(Error::Integrity("foreign read barrier".into()));
        }
        self.catch_up(token.offset)?;
        Ok(token)
    }
    fn forward(&self, path: &str, body: Value, id: Option<&str>) -> Result<Value> {
        let owner = self.catalog.owner()?;
        let mut request = self
            .client
            .post(format!("{}{path}", owner.address))
            .bearer_auth(&self.config.token)
            .json(&body);
        if let Some(id) = id {
            request = request.header("idempotency-key", id);
        }
        let response = request.send()?;
        let value: Value = response.json()?;
        // Application errors remain structured when passing through a query gateway.
        if value.get("code").is_none() {
            return Err(Error::Unavailable("invalid writer response".into()));
        }
        Ok(value)
    }
    fn execute(&mut self, operation: &str, request: Value, id: Option<&str>) -> Result<Work> {
        if let Some(action) = operation.strip_prefix("indexes/")
            && ["build", "retry", "cancel", "get_status"].contains(&action)
        {
            let data = self.replica.index_control(
                if action == "get_status" {
                    "status"
                } else {
                    action
                },
                &request,
            )?;
            return Ok(Work::Ready(json!({"code":0,"data":data})));
        }
        let write = matches!(
            operation,
            "collections/create"
                | "entities/insert"
                | "entities/upsert"
                | "entities/delete"
                | "indexes/create"
                | "indexes/drop"
                | "partitions/create"
        );
        if write {
            let id = id.ok_or_else(|| Error::Invalid("Idempotency-Key header required".into()))?;
            if self.lease.is_none() {
                return self
                    .forward(&format!("/v2/vectordb/{operation}"), request, Some(id))
                    .map(Work::Ready);
            }
            self.guard()?;
            match self
                .replica
                .prepare(self.replica.epoch, id, operation, request)?
            {
                Err(result) => Ok(Work::Ready(
                    json!({"code":0,"data":result.data,"commitToken":result.token}),
                )),
                Ok(record) => {
                    self.commit(record)?;
                    let result = self
                        .replica
                        .result(id)
                        .ok_or_else(|| Error::Integrity("committed result missing".into()))?;
                    Ok(Work::Ready(
                        json!({"code":0,"data":result.data,"commitToken":result.token}),
                    ))
                }
            }
        } else {
            let token = self.barrier()?;
            let view = self.replica.read_view(operation, &request)?;
            Ok(Work::Read {
                view: Box::new(view),
                token,
                operation: operation.into(),
                request,
            })
        }
    }
    fn checkpoint(&mut self) -> Result<Value> {
        self.barrier()?;
        if let Err(error) = self.replica.build_indexes() {
            eprintln!("checkpoint retains FLAT fallback: {error}");
        }
        self.flush_state()
    }
    fn flush_state(&mut self) -> Result<Value> {
        let snapshot = self.replica.checkpoint()?;
        let prefix = format!(
            "{}/checkpoints/{}-{}",
            self.replica.scope.prefix(),
            self.replica.offset,
            uuid::Uuid::new_v4()
        );
        let mut keys = vec![];
        for (key, bytes) in snapshot.sealed.objects {
            let key = format!("{prefix}/{key}");
            let hash = digest(&bytes);
            self.objects.put(&key, bytes)?;
            if digest(&self.objects.get(&key)?) != hash {
                return Err(Error::Integrity(
                    "uploaded segment verification failed".into(),
                ));
            }
            keys.push(key);
        }
        let bytes = serde_json::to_vec(&snapshot.checkpoint)?;
        let manifest = Manifest {
            scope: self.replica.scope.clone(),
            offset: self.replica.offset,
            key: format!("{prefix}/state.json"),
            sha256: digest(&bytes),
            bytes: bytes.len(),
            objects: keys,
        };
        self.objects.put(&manifest.key, bytes)?;
        if digest(&self.objects.get(&manifest.key)?) != manifest.sha256 {
            return Err(Error::Integrity(
                "uploaded checkpoint verification failed".into(),
            ));
        }
        let published = self.catalog.publish(&manifest)?;
        if published {
            self.replica.reclaim();
            self.log.trim(manifest.offset)?;
            self.catalog
                .collect_retired(|key| self.objects.delete(key))?;
        } else {
            for key in &manifest.objects {
                self.objects.delete(key)?;
            }
            self.objects.delete(&manifest.key)?;
        }
        self.uncheckpointed_records = 0;
        Ok(json!({"published":published,"manifest":manifest}))
    }
}
impl Drop for Runtime {
    fn drop(&mut self) {
        self.failed.store(true, Ordering::SeqCst);
        self.replica.stop_index_jobs();
        if let Some(lease) = &self.lease {
            let _ = self.catalog.revoke(&lease.id);
        }
    }
}
#[derive(Clone)]
struct App {
    runtime: Arc<Mutex<Runtime>>,
    token: Arc<String>,
    slots: Arc<tokio::sync::Semaphore>,
}
fn authorized(headers: &HeaderMap, token: &str) -> bool {
    let expected = format!("Bearer {token}");
    let supplied = headers
        .get("authorization")
        .and_then(|h| h.to_str().ok())
        .unwrap_or("");
    let mut difference = expected.len() ^ supplied.len();
    for (i, byte) in expected.bytes().enumerate() {
        difference |= (byte ^ supplied.as_bytes().get(i).copied().unwrap_or(0)) as usize;
    }
    difference == 0
}
fn response(value: Value) -> Response {
    let status = value["status"]
        .as_u64()
        .and_then(|n| u16::try_from(n).ok())
        .and_then(|n| StatusCode::from_u16(n).ok())
        .unwrap_or(StatusCode::OK);
    (status, Json(value)).into_response()
}
fn error_response(error: Error) -> Response {
    let status = match &error {
        Error::Invalid(_) | Error::Engine(vectordb_engine::Error::Invalid(_)) => {
            StatusCode::BAD_REQUEST
        }
        Error::Conflict(_) | Error::Engine(vectordb_engine::Error::Conflict(_)) => {
            StatusCode::CONFLICT
        }
        Error::Unsupported(_) | Error::Engine(vectordb_engine::Error::Unsupported(_)) => {
            StatusCode::NOT_IMPLEMENTED
        }
        Error::Engine(vectordb_engine::Error::NotFound(_)) => StatusCode::NOT_FOUND,
        _ => StatusCode::SERVICE_UNAVAILABLE,
    };
    let message = if status == StatusCode::SERVICE_UNAVAILABLE {
        eprintln!("cluster operation failed: {error}");
        "cluster unavailable; write outcome may be unknown, retry with the same idempotency key"
            .into()
    } else {
        error.to_string()
    };
    (
        status,
        Json(json!({"code":status.as_u16(),"status":status.as_u16(),"message":message})),
    )
        .into_response()
}
async fn run(
    app: App,
    headers: HeaderMap,
    action: impl FnOnce(&mut Runtime) -> Result<Work> + Send + 'static,
) -> Response {
    if !authorized(&headers, &app.token) {
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
        let work = {
            let mut runtime = app
                .runtime
                .lock()
                .map_err(|_| Error::Unavailable("worker panicked".into()))?;
            action(&mut runtime)?
        };
        match work {
            Work::Ready(value) => Ok(value),
            Work::Read {
                view,
                token,
                operation,
                request,
            } => {
                let data = view.execute(&operation, request)?;
                Ok(json!({"code":0,"data":data,"readToken":token}))
            }
        }
    })
    .await
    {
        Ok(Ok(value)) => response(value),
        Ok(Err(error)) => error_response(error),
        Err(_) => error_response(Error::Unavailable("worker failed".into())),
    }
}
async fn execute(
    State(app): State<App>,
    Path((category, action)): Path<(String, String)>,
    headers: HeaderMap,
    Json(body): Json<Value>,
) -> Response {
    let id = headers
        .get("idempotency-key")
        .and_then(|h| h.to_str().ok())
        .map(str::to_owned);
    run(app, headers, move |runtime| {
        runtime.execute(&format!("{category}/{action}"), body, id.as_deref())
    })
    .await
}
async fn barrier(State(app): State<App>, headers: HeaderMap) -> Response {
    run(app, headers, |runtime| {
        Ok(Work::Ready(json!({"code":0,"data":runtime.barrier()?})))
    })
    .await
}
async fn ready(State(app): State<App>, headers: HeaderMap) -> Response {
    run(app, headers, |runtime| {
        runtime.barrier()?;
        Ok(Work::Ready(json!({"code":0,"ready":true})))
    })
    .await
}
#[tokio::main]
async fn main() -> std::result::Result<(), Box<dyn std::error::Error>> {
    let config = Config::parse();
    let token = Arc::new(config.token.clone());
    let bind = config.bind;
    let checkpoint = config.role == Role::Checkpoint;
    let mut runtime = tokio::task::spawn_blocking(move || Runtime::open(config)).await??;
    if checkpoint {
        let result = tokio::task::spawn_blocking(move || runtime.checkpoint()).await??;
        println!("{}", serde_json::to_string(&result)?);
        return Ok(());
    }
    let state = App {
        runtime: Arc::new(Mutex::new(runtime)),
        token,
        slots: Arc::new(tokio::sync::Semaphore::new(32)),
    };
    let weak = Arc::downgrade(&state.runtime);
    std::thread::spawn(move || {
        loop {
            let Some(runtime) = weak.upgrade() else { break };
            let jobs = {
                let Ok(runtime) = runtime.lock() else { break };
                if runtime.failed.load(Ordering::Acquire) {
                    break;
                }
                runtime.replica.index_jobs()
            };
            drop(runtime);
            for job in jobs {
                std::thread::spawn(move || job.run());
            }
            std::thread::sleep(Duration::from_millis(100));
        }
    });
    let app = Router::new()
        .route("/healthz", get(|| async { Json(json!({"status":"alive","mode":"cluster-m2"})) }))
        .route("/readyz", get(ready))
        .route("/capabilities", get(|| async { Json(json!({"mode":"cluster-m2","productionReady":false,"atomicScope":"collection","queryShards":1,"consistency":["Strong"],"checkpointFormat":"state-segments-v3","persistentIndexes":true,"concurrentReads":true,"automaticFailover":false,"idempotencyWindowMs":86400000,"idempotencyKeyFormat":"v2-<issuedUnixMs>-<nonce>","defaultVectorDtype":"Float32","explicitPrimaryKeyDelete":true,"automaticCheckpointRecords":128})) }))
        .route("/internal/barrier", post(barrier))
        .route("/v2/vectordb/{category}/{action}", post(execute))
        .layer(DefaultBodyLimit::max(512 * 1024))
        .with_state(state.clone());
    let listener = tokio::net::TcpListener::bind(bind).await?;
    eprintln!(
        "Experimental cluster process listening on {}",
        listener.local_addr()?
    );
    axum::serve(listener, app)
        .with_graceful_shutdown(async {
            let ctrl_c = async {
                let _ = tokio::signal::ctrl_c().await;
            };
            #[cfg(unix)]
            let terminate = async {
                if let Ok(mut signal) =
                    tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())
                {
                    signal.recv().await;
                }
            };
            #[cfg(not(unix))]
            let terminate = std::future::pending::<()>();
            tokio::select! { _ = ctrl_c => {}, _ = terminate => {} }
        })
        .await?;
    // reqwest's blocking client and Kafka shutdown must not be dropped on an async worker.
    tokio::task::spawn_blocking(move || drop(state)).await?;
    Ok(())
}
