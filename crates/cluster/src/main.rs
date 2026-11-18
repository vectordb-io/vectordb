mod admin;
mod node;
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
    catalog::{Catalog, Lease, Manifest, OwnerPhase},
    digest,
    kafka::Log,
    object::Objects,
    state::{Entry, FORMAT, Record, Replica, Scope, Token},
};

#[derive(Clone, Debug, ValueEnum, PartialEq)]
enum Role {
    Node,
    Coordinator,
    QueryNode,
    Writer,
    Query,
    Checkpoint,
    Backup,
    Restore,
}
#[derive(Parser, Clone)]
#[command(about = "Distributed VectorDB")]
struct Config {
    #[arg(long, value_enum)]
    role: Role,
    #[arg(long, env = "VECTORDB_CLUSTER", default_value = "development")]
    cluster: String,
    #[arg(long, env = "VECTORDB_COLLECTION", default_value = "")]
    collection: String,
    #[arg(long, env = "VECTORDB_GENERATION", default_value = "")]
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
    #[arg(long, env = "VECTORDB_WAL_REPLICAS", default_value_t = 1)]
    wal_replicas: i32,
    #[arg(long, env = "VECTORDB_COORDINATED", default_value_t = false)]
    coordinated: bool,
    #[arg(long, env = "VECTORDB_NODE_ID", default_value = "")]
    node_id: String,
    #[arg(long, env = "VECTORDB_ZONE", default_value = "")]
    zone: String,
    #[arg(long, env = "VECTORDB_RPC_BIND")]
    rpc_bind: Option<SocketAddr>,
    #[arg(long, env = "VECTORDB_RPC_ADVERTISE")]
    rpc_advertise: Option<String>,
    #[arg(long, env = "VECTORDB_TLS_CA")]
    tls_ca: Option<String>,
    #[arg(long, env = "VECTORDB_TLS_CERT")]
    tls_cert: Option<String>,
    #[arg(long, env = "VECTORDB_TLS_KEY", hide_env_values = true)]
    tls_key: Option<String>,
    #[arg(long, env = "VECTORDB_QUERY_REPLICAS", default_value_t = 2)]
    query_replicas: usize,
    #[arg(long, env = "VECTORDB_MAINTENANCE_MS", default_value_t = 60000)]
    maintenance_ms: i64,
    #[arg(long, env = "VECTORDB_MAX_SHARDS", default_value_t = 128)]
    max_shards: usize,
    #[arg(long, env = "VECTORDB_READ_TOKEN", hide_env_values = true)]
    read_token: Option<String>,
    #[arg(long, env = "VECTORDB_WRITE_TOKEN", hide_env_values = true)]
    write_token: Option<String>,
    #[arg(long, env = "VECTORDB_REQUESTS_PER_SECOND", default_value_t = 1000)]
    requests_per_second: u32,
    #[arg(long, env = "VECTORDB_NAMESPACE_COLLECTIONS", default_value_t = 16)]
    namespace_collections: usize,
    #[arg(long,env="VECTORDB_COLLECTION_BYTES",default_value_t=vectordb_engine::segment::MAX_STATE_BYTES)]
    collection_bytes: usize,
    #[arg(long, env = "VECTORDB_WAL_GRACE_OFFSETS")]
    wal_grace_offsets: Option<u32>,
    #[arg(long)]
    archive: Option<std::path::PathBuf>,
    #[arg(long)]
    gateway: Option<String>,
    #[arg(skip)]
    incarnation: String,
    #[arg(skip)]
    rpc: Option<vectordb_cluster::transport::Client>,
    #[arg(skip)]
    write_channels: Option<usize>,
    #[arg(skip)]
    managed: bool,
}
fn advertised_origin(address: Option<&str>) -> Result<String> {
    let address =
        address.ok_or_else(|| Error::Invalid("writer/node requires --advertise origin".into()))?;
    let url =
        reqwest::Url::parse(address).map_err(|_| Error::Invalid("invalid advertise URL".into()))?;
    if !matches!(url.scheme(), "http" | "https")
        || !url.username().is_empty()
        || url.password().is_some()
        || url.query().is_some()
        || url.fragment().is_some()
        || url.path() != "/"
    {
        return Err(Error::Invalid("advertise must be an HTTP(S) origin".into()));
    }
    Ok(address.trim_end_matches('/').into())
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
struct ReplayTask {
    target: std::sync::mpsc::Sender<i64>,
    worker: std::thread::JoinHandle<Result<Replica>>,
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
    write_head: vectordb_cluster::decisions::WriteHead,
    replay_task: Option<ReplayTask>,
    publication: Option<std::thread::JoinHandle<Result<Value>>>,
    uncheckpointed_records: usize,
    directory: Option<vectordb_cluster::directory::Directory>,
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
        let mut replica = Self::load_checkpoint(&catalog, &objects, &scope)?
            .unwrap_or(Replica::empty(scope.clone())?);
        replica.limit_writes(config.collection_bytes);
        let directory = if config.managed {
            Some(vectordb_cluster::directory::Directory::new(
                config.etcd.clone(),
                &config.cluster,
            )?)
        } else {
            None
        };
        let mut runtime = Self {
            config,
            write_head: Default::default(),
            replay_task: None,
            publication: None,
            uncheckpointed_records: 0,
            directory,
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
            let address = advertised_origin(runtime.config.advertise.as_deref())?;
            let lease = runtime.catalog.acquire(if runtime.config.managed {
                format!(
                    "{}/internal/collections/{}/{}",
                    address.trim_end_matches('/'),
                    runtime.config.collection,
                    runtime.config.generation
                )
            } else {
                address.trim_end_matches('/').into()
            })?;
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
            node::failpoint("owner_assigned");
            if runtime.config.managed {
                runtime.log.ensure_topic(runtime.config.wal_replicas)?;
            }
            if let Some(n) = runtime.config.write_channels {
                runtime.log.participants.ensure(
                    &runtime.replica.scope,
                    n,
                    runtime.config.wal_replicas,
                )?;
            }
            runtime
                .catalog
                .phase(runtime.lease.as_ref().unwrap(), OwnerPhase::Recovering)?;
            node::failpoint("owner_recovering");
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
            let prior = runtime.replica.offset;
            runtime.catch_up(offset)?;
            // Count the recovered tail too. Restarting a writer cannot reset WAL backpressure.
            runtime.uncheckpointed_records = usize::try_from(offset.saturating_sub(prior))
                .unwrap_or(usize::MAX)
                .min(128);
            if runtime.uncheckpointed_records >= 128 {
                runtime.flush_state()?;
            }
            node::failpoint("owner_activated");
            runtime.guard()?;
            runtime
                .catalog
                .phase(runtime.lease.as_ref().unwrap(), OwnerPhase::Active)?;
            node::failpoint("owner_ready");
        }
        Ok(runtime)
    }
    fn guard(&self) -> Result<()> {
        if self.config.coordinated && self.lease.is_some() {
            let control = &self.config.rpc.as_ref().unwrap().control;
            if let Err(e) = control.authorize_writer(
                &self.replica.scope,
                &self.config.node_id,
                &self.config.incarnation,
            ) {
                self.failed.store(true, Ordering::SeqCst);
                return Err(e);
            }
        }
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
            self.replica.limit_writes(self.config.collection_bytes);
            return self
                .log
                .replay(self.replica.offset, offset, |position, record| {
                    self.replica.apply(position, record)
                });
        }
        result
    }
    fn advance_replay(&mut self, offset: i64) {
        // Start only for sustained writes. At most one worker follows an immutable
        // base and the bounded (128-record) decision tail for this collection.
        if self.replay_task.is_none() && self.uncheckpointed_records >= 16 {
            let mut replica = self.replica.clone();
            let log = Log::reader(self.config.kafka.clone(), &replica.scope);
            let (target, work) = std::sync::mpsc::channel::<i64>();
            let worker = std::thread::spawn(move || {
                while let Ok(mut end) = work.recv() {
                    for newer in work.try_iter() {
                        end = newer;
                    }
                    log.replay(replica.offset, end, |n, r| replica.apply(n, r))?;
                }
                Ok(replica)
            });
            self.replay_task = Some(ReplayTask { target, worker });
        }
        if let Some(task) = &self.replay_task {
            let _ = task.target.send(offset);
        }
    }
    fn materialize(&mut self) -> Result<()> {
        if let Some(end) = self.write_head.offset {
            if let Some(task) = self.replay_task.take() {
                drop(task.target);
                // A failed prefetch has no persistent effects. Resume from our
                // verified base using the normal checkpoint/WAL recovery path.
                if let Ok(Ok(r)) = task.worker.join() {
                    self.replica = r;
                }
            }
            self.catch_up(end)
                .inspect_err(|_| self.failed.store(true, Ordering::SeqCst))?;
            self.write_head = Default::default();
        }
        Ok(())
    }
    fn decide(&mut self, vote: vectordb_cluster::channels::Vote) -> Result<Value> {
        self.guard()?;
        if self.lease.is_none() {
            return Err(Error::Conflict("decision owner required".into()));
        }
        if let Some(directory) = &self.directory {
            directory.allow_mutations()?;
        }
        let result = match self.write_head.prepare(
            &self.replica,
            vote,
            self.config.collection_bytes,
            vectordb_cluster::state::now_ms(),
        )? {
            Err(known) => known,
            Ok(plan) => {
                node::failpoint("before_channel_decision");
                let committed = (|| {
                    let offset = self.log.decide(&plan.decision)?;
                    node::failpoint("channel_decided");
                    if self.publication.is_some() {
                        node::failpoint("channel_next_decision_after_cut");
                    }
                    if node::injected("mutation_unknown") {
                        return Err(Error::Unavailable("unknown decision outcome".into()));
                    }
                    let r = self.write_head.committed(plan, offset)?;
                    self.guard()?;
                    self.uncheckpointed_records += 1;
                    self.advance_replay(offset);
                    if self.uncheckpointed_records >= 128 {
                        self.automatic_flush()?;
                    }
                    Ok(r)
                })();
                if committed.is_err() {
                    self.failed.store(true, Ordering::SeqCst);
                }
                committed?
            }
        };
        Ok(json!({"code":0,"data":result.data,"commitToken":result.token}))
    }
    fn commit(&mut self, record: Record) -> Result<i64> {
        self.materialize()?;
        self.guard()?;
        if matches!(record.entry, Entry::Mutation { .. })
            && let Some(directory) = &self.directory
        {
            directory.allow_mutations()?;
        }
        self.replica.check_capacity(&record)?;
        let result = (|| {
            let offset = self.log.append(&record)?;
            if matches!(record.entry, Entry::Mutation { .. }) {
                node::failpoint("mutation_committed");
                if node::injected("mutation_unknown") {
                    return Err(Error::Unavailable("injected unknown commit outcome".into()));
                }
            }
            self.replica.apply(offset, record)?;
            self.guard()?;
            self.uncheckpointed_records += 1;
            if self.uncheckpointed_records >= 128 {
                self.automatic_flush()?;
            }
            Ok(offset)
        })();
        if result.is_err() {
            self.failed.store(true, Ordering::SeqCst);
        }
        result
    }
    // Sharded readers resolve the same decision prefix on their own Query processes.
    fn query_snapshot(&mut self) -> Result<(Token, vectordb_engine::segment::CollectionSpec)> {
        let mut spec = self
            .replica
            .spec()
            .ok_or_else(|| Error::Conflict("collection dropped before snapshot".into()))?
            .clone();
        if let Some(next) = self.write_head.next_id() {
            self.guard()?;
            spec.next_id = next;
            let record = Record {
                format: FORMAT,
                scope: self.replica.scope.clone(),
                entry: Entry::Barrier {
                    epoch: self.replica.epoch,
                },
            };
            let offset = self
                .log
                .append(&record)
                .inspect_err(|_| self.failed.store(true, Ordering::SeqCst))?;
            self.write_head.offset = Some(offset);
            self.guard()?;
            self.uncheckpointed_records += 1;
            if self.uncheckpointed_records >= 128 {
                self.automatic_flush()?;
            }
            return Ok((
                Token {
                    scope: self.replica.scope.clone(),
                    offset,
                },
                spec,
            ));
        }
        Ok((self.barrier()?, spec))
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
        let owner = self.catalog.active_owner()?;
        if let Some(rpc) = &self.config.rpc {
            return rpc.call(
                &format!("{}{path}", owner.address),
                body,
                id,
                vectordb_cluster::state::now_ms() + 30_000,
            );
        }
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
        if operation == "collections/drop" && !self.config.managed {
            return Err(Error::Unsupported(
                "collection lifecycle requires --role node".into(),
            ));
        }
        if operation == "collections/storage_stats" {
            if self.config.write_channels.is_some() && self.lease.is_none() {
                return self
                    .forward("/v2/vectordb/collections/storage_stats", request, None)
                    .map(Work::Ready);
            }
            let mut count = 0_u64;
            let mut bytes = 0_u64;
            self.objects.visit_checkpoint_objects(
                &format!("{}/checkpoints/", self.replica.scope.prefix()),
                |_, _, size| {
                    count += 1;
                    bytes += size;
                    Ok(())
                },
            )?;
            let (low, high) = self.log.watermarks()?;
            let channels = self
                .config
                .write_channels
                .map(|n| self.log.participants.watermarks(&self.replica.scope, n))
                .transpose()?;
            return Ok(Work::Ready(
                json!({"code":0,"data":{"prepareWatermarks":channels,"prepareFloors":self.replica.channel_floors,"objectBytes":bytes,"objectCount":count,"walLow":low,"walHigh":high,"checkpointOffset":self.catalog.manifest()?.map(|m|m.offset),"stateBytes":self.write_head.bytes().unwrap_or_else(||self.replica.state_bytes())}}),
            ));
        }
        if matches!(operation, "collections/flush" | "collections/compact") {
            self.barrier()?;
            self.replica.read_view("collections/describe", &request)?;
            return Ok(Work::Ready(json!({"code":0,"data":self.checkpoint()?})));
        }
        if let Some(action) = operation.strip_prefix("indexes/")
            && ["build", "retry", "cancel", "get_status"].contains(&action)
        {
            self.barrier()?;
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
                | "collections/drop"
                | "partitions/drop"
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
            self.write(operation, request, id)
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
    fn write(&mut self, operation: &str, request: Value, id: &str) -> Result<Work> {
        self.materialize()?;
        self.guard()?;
        match self
            .replica
            .prepare(self.replica.epoch, id, operation, request)?
        {
            Err(result) => Ok(Work::Ready(
                json!({"code":0,"data":result.data,"commitToken":result.token}),
            )),
            Ok(record) => {
                if self.config.write_channels.is_some()
                    && vectordb_cluster::channels::field(operation).is_some()
                {
                    return Err(Error::Invalid(
                        "channel collection requires prepared writes".into(),
                    ));
                }
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
    }
    fn checkpoint(&mut self) -> Result<Value> {
        self.barrier()?;
        if let Err(error) = self.replica.build_indexes() {
            eprintln!("checkpoint retains FLAT fallback: {error}");
        }
        self.compact()
    }
    fn compact(&mut self) -> Result<Value> {
        let result = self.flush_state()?;
        self.catalog
            .collect_orphans(&self.objects, vectordb_cluster::state::now_ms() - 300_000)?;
        Ok(result)
    }
    fn prepare_checkpoint(&mut self) -> Result<()> {
        self.materialize()?;
        if let Some(n) = self.config.write_channels
            && self.lease.is_some()
        {
            // Order the cut with mutations using the fenced decision producer. Even an old
            // checkpoint publisher can then reclaim only prepares below a durable cut.
            self.guard()?;
            let floors = self
                .log
                .participants
                .watermarks(&self.replica.scope, n)?
                .into_iter()
                .map(|(_, hi)| (hi - 32).max(0))
                .collect::<Vec<_>>();
            let cut = Record {
                format: FORMAT,
                scope: self.replica.scope.clone(),
                entry: Entry::ChannelCut {
                    epoch: self.replica.epoch,
                    floors,
                },
            };
            let offset = self
                .log
                .append(&cut)
                .inspect_err(|_| self.failed.store(true, Ordering::SeqCst))?;
            self.replica
                .apply(offset, cut)
                .inspect_err(|_| self.failed.store(true, Ordering::SeqCst))?;
            node::failpoint("channel_cut");
        }
        Ok(())
    }
    fn join_publication(&mut self) -> Result<()> {
        if let Some(job) = self.publication.take() {
            job.join()
                .map_err(|_| Error::Unavailable("checkpoint worker terminated".into()))??;
        }
        Ok(())
    }
    fn automatic_flush(&mut self) -> Result<()> {
        if self.config.write_channels.is_none() {
            self.flush_state()?;
            return Ok(());
        }
        self.join_publication()?;
        self.prepare_checkpoint()?;
        let replica = self.replica.clone();
        let objects = self.objects.clone();
        let catalog = self.catalog.clone();
        let log = Log::reader(self.config.kafka.clone(), &replica.scope);
        let grace = self.wal_grace();
        self.publication = Some(std::thread::spawn(move || {
            publish_checkpoint(&replica, &objects, &catalog, &log, grace)
        }));
        self.uncheckpointed_records = 0;
        Ok(())
    }
    fn wal_grace(&self) -> u32 {
        self.config
            .wal_grace_offsets
            .unwrap_or(if self.config.coordinated { 64 } else { 0 })
    }
    fn flush_state(&mut self) -> Result<Value> {
        self.join_publication()?;
        self.prepare_checkpoint()?;
        let result = publish_checkpoint(
            &self.replica,
            &self.objects,
            &self.catalog,
            &self.log,
            self.wal_grace(),
        )?;
        if result["published"] == true && self.config.write_channels.is_none() {
            self.replica.reclaim();
        }
        self.uncheckpointed_records = 0;
        Ok(result)
    }
}
fn publish_checkpoint(
    replica: &Replica,
    objects: &Objects,
    catalog: &Catalog,
    log: &Log,
    grace: u32,
) -> Result<Value> {
    let snapshot = replica.checkpoint()?;
    let manifest = upload_snapshot(objects, snapshot)?;
    let published = catalog.publish(&manifest)?;
    if published {
        node::failpoint("checkpoint_published");

        let through = manifest.offset - i64::from(grace);
        if through >= 0 {
            log.trim(through)?;
            node::failpoint("wal_trimmed");
        }
        log.participants
            .trim(&replica.scope, &replica.channel_floors)?;
        catalog.collect_retired(|key| objects.delete(key))?;
    } else {
        for key in &manifest.objects {
            objects.delete(key)?;
        }
        objects.delete(&manifest.key)?;
    }

    Ok(json!({"published":published,"manifest":manifest}))
}
fn upload_snapshot(
    objects: &Objects,
    snapshot: vectordb_cluster::state::Snapshot,
) -> Result<Manifest> {
    let prefix = format!(
        "{}/checkpoints/{}-{}",
        snapshot.checkpoint.scope.prefix(),
        snapshot.checkpoint.offset,
        uuid::Uuid::new_v4()
    );
    let mut keys = vec![];
    for (key, bytes) in snapshot.sealed.objects {
        let key = format!("{prefix}/{key}");
        let hash = digest(&bytes);
        objects.put(&key, bytes)?;
        if digest(&objects.get(&key)?) != hash {
            return Err(Error::Integrity(
                "uploaded segment verification failed".into(),
            ));
        }
        keys.push(key);
    }
    let bytes = serde_json::to_vec(&snapshot.checkpoint)?;
    let manifest = Manifest {
        scope: snapshot.checkpoint.scope.clone(),
        offset: snapshot.checkpoint.offset,
        key: format!("{prefix}/state.json"),
        sha256: digest(&bytes),
        bytes: bytes.len(),
        objects: keys,
    };
    objects.put(&manifest.key, bytes)?;
    node::failpoint("checkpoint_uploaded");
    if digest(&objects.get(&manifest.key)?) != manifest.sha256 {
        return Err(Error::Integrity(
            "uploaded checkpoint verification failed".into(),
        ));
    }
    Ok(manifest)
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
    let mut config = Config::parse();
    if matches!(config.role, Role::Backup | Role::Restore) {
        tokio::task::spawn_blocking(move || admin::run(config)).await??;
        return Ok(());
    }
    if config.role == Role::Coordinator {
        config.coordinated = true;
    }
    if matches!(
        config.role,
        Role::Node | Role::QueryNode | Role::Coordinator
    ) {
        return node::serve(config).await;
    }
    if config.coordinated {
        return Err("--coordinated requires node, query-node, or coordinator role".into());
    }
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
