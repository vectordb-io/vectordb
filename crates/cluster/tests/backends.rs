//! Run only against the dedicated development Compose stack.
use vectordb_cluster::{
    catalog::{Catalog, Manifest},
    digest,
    object::Objects,
    state::Scope,
};

#[test]
#[ignore = "requires isolated etcd and S3 from deployments/cluster/compose.yaml"]
fn real_catalog_ownership_cas_and_immutable_objects() {
    let scope = Scope {
        cluster: "backend-test".into(),
        collection: "items".into(),
        generation: uuid::Uuid::new_v4().to_string(),
    };
    let catalog = Catalog::new("http://127.0.0.1:12379".into(), &scope).unwrap();
    let lease = catalog.acquire("http://127.0.0.1:19531".into()).unwrap();
    assert!(catalog.acquire("http://127.0.0.1:19532".into()).is_err());
    catalog.renew(&lease).unwrap();
    catalog.verify(&lease).unwrap();
    let objects = Objects::new(
        "http://127.0.0.1:19000".into(),
        "vectordb".into(),
        "us-east-1".into(),
        "development".into(),
        "development-only-password".into(),
    )
    .unwrap();
    let key = format!("{}/checkpoints/10-first/object.json", scope.prefix());
    let bytes = b"immutable test object".to_vec();
    let store: &dyn vectordb_cluster::contracts::ObjectStore = &objects;
    store.put_new(&key, bytes.clone()).unwrap();
    assert_eq!(store.get(&key).unwrap(), bytes);
    assert!(store.put_new(&key, b"overwrite".to_vec()).is_err());
    assert_eq!(objects.get(&key).unwrap(), bytes);
    assert!(objects.get("../escape").is_err());
    let mut manifest = Manifest {
        scope,
        offset: 10,
        key,
        sha256: digest(&bytes),
        bytes: bytes.len(),
        objects: vec![],
    };
    let boundary: &dyn vectordb_cluster::contracts::Catalog = &catalog;
    assert!(boundary.publish(&manifest).unwrap());
    assert_eq!(boundary.latest().unwrap().unwrap().offset, 10);
    let mut foreign = manifest.clone();
    foreign.scope.generation = "foreign".into();
    assert!(boundary.publish(&foreign).is_err());
    manifest.offset = 9;
    let original_key = manifest.key.clone();
    manifest.key = format!(
        "{}/checkpoints/9-stale/object.json",
        manifest.scope.prefix()
    );
    assert!(!catalog.publish(&manifest).unwrap());
    manifest.key = original_key;
    assert_eq!(catalog.manifest().unwrap().unwrap().offset, 10);
    let (pinned, pin, pin_lease) = catalog.pin_latest().unwrap().unwrap();
    catalog.verify_pin(&pin).unwrap();
    manifest.offset = 11;
    manifest.key = format!("{}/checkpoints/11-second/new.json", manifest.scope.prefix());
    objects.put(&manifest.key, bytes.clone()).unwrap();
    assert!(catalog.publish(&manifest).unwrap());
    assert_eq!(
        catalog.collect_retired(|key| objects.delete(key)).unwrap(),
        0
    );
    let orphan = format!(
        "{}/checkpoints/9-abandoned/partial.bin",
        manifest.scope.prefix()
    );
    objects.put(&orphan, vec![42]).unwrap();
    assert_eq!(catalog.collect_orphans(&objects, 0).unwrap(), 0);
    assert_eq!(catalog.collect_orphans(&objects, i64::MAX).unwrap(), 1);
    assert!(objects.get(&orphan).is_err());
    assert!(objects.get(&pinned.key).is_ok());
    catalog.revoke(&pin_lease).unwrap();
    assert!(catalog.verify_pin(&pin).is_err());
    assert_eq!(
        catalog.collect_retired(|key| objects.delete(key)).unwrap(),
        1
    );
    assert!(objects.get(&pinned.key).is_err());
    assert!(objects.get(&manifest.key).is_ok());
    catalog.revoke(&lease.id).unwrap();
    assert!(catalog.verify(&lease).is_err());
    let next = catalog.acquire("http://127.0.0.1:19532".into()).unwrap();
    assert!(next.epoch > lease.epoch);
    assert!(catalog.verify(&lease).is_err());
    catalog.revoke(&next.id).unwrap();
}
#[test]
#[ignore = "requires isolated Kafka from deployments/cluster/compose.yaml"]
fn real_log_committed_prefix_and_broker_producer_fencing() {
    use rdkafka::{
        ClientConfig,
        admin::{AdminClient, AdminOptions, NewTopic, TopicReplication},
        client::DefaultClientContext,
    };
    use vectordb_cluster::{
        contracts::DurableLog,
        kafka::Log,
        state::{Entry, FORMAT, Record},
    };
    let scope = Scope {
        cluster: "contract".into(),
        collection: "items".into(),
        generation: uuid::Uuid::new_v4().to_string(),
    };
    let topic = scope.topic();
    let admin: AdminClient<DefaultClientContext> = ClientConfig::new()
        .set("bootstrap.servers", "127.0.0.1:19092")
        .create()
        .unwrap();
    let created = futures::executor::block_on(admin.create_topics(
        &[NewTopic::new(&topic, 1, TopicReplication::Fixed(1))],
        &AdminOptions::new(),
    ))
    .unwrap();
    assert!(created[0].is_ok());
    let mut old = Log::reader("127.0.0.1:19092".into(), &scope);
    old.initialize_writer().unwrap();
    let first = Record {
        format: FORMAT,
        scope: scope.clone(),
        entry: Entry::Activate { epoch: 1 },
    };
    let offset = DurableLog::append(&old, &first).unwrap();
    let mut next = Log::reader("127.0.0.1:19092".into(), &scope);
    next.initialize_writer().unwrap();
    assert!(
        DurableLog::append(&old, &first).is_err(),
        "broker must fence the old producer independently of etcd checks"
    );
    let through = DurableLog::append(
        &next,
        &Record {
            format: FORMAT,
            scope: scope.clone(),
            entry: Entry::Activate { epoch: 2 },
        },
    )
    .unwrap();
    let mut seen = vec![];
    DurableLog::replay(&next, -1, through, &mut |offset, _| {
        seen.push(offset);
        Ok(())
    })
    .unwrap();
    assert_eq!(seen, vec![offset, through]);
    DurableLog::trim(&next, offset).unwrap();
    assert!(matches!(
        DurableLog::replay(&next, -1, through, &mut |_, _| Ok(())),
        Err(vectordb_cluster::Error::CheckpointRequired)
    ));
    let deleted =
        futures::executor::block_on(admin.delete_topics(&[&topic], &AdminOptions::new())).unwrap();
    assert!(deleted[0].is_ok());
}

#[test]
#[ignore = "requires isolated etcd from deployments/cluster/compose.yaml"]
fn real_directory_receipts_cas_and_owner_recovery_phases() {
    use serde_json::json;
    use vectordb_cluster::{
        catalog::OwnerPhase,
        directory::{Directory, Lifecycle},
        state::{idempotency_key, now_ms},
    };
    let cluster = format!("m3-{}", uuid::Uuid::new_v4().simple());
    let d = Directory::new("http://127.0.0.1:12379".into(), &cluster).unwrap();
    let request = json!({"collectionName":"items","dimension":2});
    let key = idempotency_key(now_ms(), "create");
    let a = d.begin("collections/create", &request, &key).unwrap();
    let b = d.begin("collections/create", &request, &key).unwrap();
    assert_eq!(a.target, b.target);
    assert!(
        d.begin(
            "collections/create",
            &json!({"collectionName":"items","dimension":3}),
            &key
        )
        .is_err()
    );
    let scope = a.target.unwrap();
    let snapshot = d.snapshot().unwrap();
    d.finish(
        &scope,
        Lifecycle::Active,
        snapshot.by_scope(&scope).unwrap().spec,
        1,
    )
    .unwrap();
    let alias = json!({"collectionName":"items","aliasName":"live"});
    d.begin(
        "aliases/create",
        &alias,
        &idempotency_key(now_ms(), "alias"),
    )
    .unwrap();
    let request = json!({"collectionName":"live","data":[{"id":1,"vector":[1,0]}]});
    let write_id = idempotency_key(now_ms(), "entity");
    let binding = d.begin("entities/insert", &request, &write_id).unwrap();
    let replacement = d
        .begin(
            "collections/create",
            &json!({"collectionName":"second","dimension":2}),
            &idempotency_key(now_ms(), "second"),
        )
        .unwrap()
        .target
        .unwrap();
    d.finish(
        &replacement,
        Lifecycle::Active,
        d.snapshot().unwrap().by_scope(&replacement).unwrap().spec,
        1,
    )
    .unwrap();
    d.begin(
        "aliases/alter",
        &json!({"collectionName":"second","aliasName":"live"}),
        &idempotency_key(now_ms(), "switch"),
    )
    .unwrap();
    assert_eq!(
        d.begin("entities/insert", &request, &write_id)
            .unwrap()
            .target,
        binding.target
    );
    let c = Catalog::new("http://127.0.0.1:12379".into(), &scope).unwrap();
    let lease = c.acquire("http://127.0.0.1:1".into()).unwrap();
    assert!(c.active_owner().is_err());
    assert!(c.phase(&lease, OwnerPhase::Active).is_err());
    c.phase(&lease, OwnerPhase::Recovering).unwrap();
    assert!(c.active_owner().is_err());
    c.phase(&lease, OwnerPhase::Active).unwrap();
    assert_eq!(c.active_owner().unwrap().nonce, lease.owner.nonce);
    c.revoke(&lease.id).unwrap();
    assert!(c.phase(&lease, OwnerPhase::Recovering).is_err());
    let concurrent = (0..8)
        .map(|i| {
            let d = d.clone();
            std::thread::spawn(move || {
                d.begin(
                    "databases/create",
                    &json!({"dbName":format!("db{i}")}),
                    &idempotency_key(now_ms(), &format!("db{i}")),
                )
                .unwrap()
            })
        })
        .collect::<Vec<_>>();
    for t in concurrent {
        t.join().unwrap();
    }
    assert_eq!(d.snapshot().unwrap().databases.len(), 9);
    // Advance past the whole receipt bucket, then roll the clock back. Expired
    // bindings have been GC'd, but may never silently bind to the switched alias.
    let later = now_ms() + vectordb_cluster::state::IDEMPOTENCY_WINDOW_MS + 120_000;
    d.begin_at(
        "databases/create",
        &json!({"dbName":"future"}),
        &idempotency_key(later, "future"),
        later,
    )
    .unwrap();
    assert!(
        d.begin_at("entities/insert", &request, &write_id, now_ms())
            .is_err()
    );
}

#[test]
#[ignore = "requires isolated etcd from deployments/cluster/compose.yaml"]
fn real_control_fences_stale_leaders_incarnations_and_atomic_route_publication() {
    use serde_json::json;
    use std::collections::BTreeMap;
    use vectordb_cluster::{
        control::{Assignment, Control, Member, NodeRole, Phase, Placement},
        directory::{Directory, Lifecycle},
        state::{idempotency_key, now_ms},
    };
    let cluster = format!("control-{}", uuid::Uuid::new_v4().simple());
    let control = Control::new("http://127.0.0.1:12379".into(), &cluster).unwrap();
    control.initialize(2, 60000).unwrap();
    assert!(control.initialize(1, 60000).is_err());
    let member = |id: &str, role| Member {
        id: id.into(),
        incarnation: format!("boot-{id}"),
        role,
        zone: id.into(),
        origin: format!("http://{id}:8000"),
        rpc: format!("https://{id}:9000"),
        protocol_min: 1,
        protocol_max: 1,
        max_shards: 32,
        max_collections: 16,
        resources: json!({}),
    };
    let w = member("w1", NodeRole::Writer);
    let q = member("q1", NodeRole::Query);
    let wl = control.register(&w).unwrap();
    let ql = control.register(&q).unwrap();
    // Simulate a future binary's registration; healthy protocol-1 peers remain usable.
    let mut future = q.clone();
    future.id = "future".into();
    future.protocol_min = 2;
    future.protocol_max = 2;
    {
        use base64::Engine;
        let b64 = base64::engine::general_purpose::STANDARD;
        let client = reqwest::blocking::Client::new();
        let grant: serde_json::Value = client
            .post("http://127.0.0.1:12379/v3/lease/grant")
            .json(&json!({"TTL":10}))
            .send()
            .unwrap()
            .json()
            .unwrap();
        client.post("http://127.0.0.1:12379/v3/kv/put").json(&json!({"key":b64.encode(format!("/vectordb/{cluster}/_control/v1/nodes/future")),"value":b64.encode(serde_json::to_vec(&future).unwrap()),"lease":grant["ID"]})).send().unwrap().error_for_status().unwrap();
        assert!(!control.members().unwrap().contains_key("future"));
    }

    assert!(control.register(&q).is_err());
    control.heartbeat(&q, &ql).unwrap();
    let directory = Directory::new("http://127.0.0.1:12379".into(), &cluster).unwrap();
    let receipt = directory
        .begin(
            "collections/create",
            &json!({"collectionName":"items","dimension":2,"shardCount":2}),
            &idempotency_key(now_ms(), "create"),
        )
        .unwrap();
    let scope = receipt.target.unwrap();
    directory
        .finish(&scope, Lifecycle::Active, None, 0)
        .unwrap();
    let c1 = member("c1", NodeRole::Coordinator);
    let c2 = member("c2", NodeRole::Coordinator);
    let c1lease = control.register(&c1).unwrap();
    let c2lease = control.register(&c2).unwrap();
    let lease = control.acquire("c1", &c1.incarnation).unwrap();
    let (rev, mut state) = control.load().unwrap();
    state.assignments.insert(
        scope.collection.clone(),
        Assignment {
            scope: scope.clone(),
            version: 1,
            route_epoch: 1,
            placement: Placement {
                writer: "w1".into(),
                replicas: vec![vec!["q1".into()]; 2],
                incarnations: BTreeMap::from([
                    ("w1".into(), w.incarnation.clone()),
                    ("q1".into(), q.incarnation.clone()),
                ]),
            },
            phase: Phase::Warming,
            attempts: 0,
            retry_at_ms: 0,
            error: None,
            maintenance_sequence: 0,
            maintenance_pending: false,
            maintenance_completed_ms: 0,
            warmed_offset: Some(2),
        },
    );
    assert!(control.save(&lease, &rev, &mut state).unwrap());
    let (rev, mut state) = control.load().unwrap();
    // Query restart between warm acknowledgement and publication invalidates the CAS.
    control.revoke(&ql).unwrap();
    let mut replacement = q.clone();
    replacement.incarnation = "second-boot".into();
    let ql = control.register(&replacement).unwrap();
    assert!(control.verify_member("q1", &q.incarnation).is_err());
    let members = BTreeMap::from([("w1".into(), w), ("q1".into(), replacement.clone())]);
    assert!(
        !control
            .publish(
                &lease,
                &rev,
                &mut state,
                &directory,
                &scope.collection,
                &members
            )
            .unwrap()
    );
    assert!(
        directory
            .snapshot()
            .unwrap()
            .by_scope(&scope)
            .unwrap()
            .query_routes
            .is_none()
    );
    // A lost coordinator lease prevents both task changes and route publication.
    // Losing membership also fences publication while the longer leader lease remains.
    control.revoke(&c1lease).unwrap();
    let (rev, mut stale) = control.load().unwrap();
    assert!(!control.save(&lease, &rev, &mut stale).unwrap());
    assert!(control.renew(&lease).is_err());
    control.revoke(&lease.id).unwrap();
    let next = control.acquire("c2", &c2.incarnation).unwrap();
    assert!(
        control
            .verify_leader("c1", &c1.incarnation, lease.epoch)
            .is_err()
    );
    let (rev, mut state) = control.load().unwrap();
    assert!(!control.save(&lease, &rev, &mut state).unwrap());
    state
        .assignments
        .get_mut(&scope.collection)
        .unwrap()
        .placement
        .incarnations
        .insert("q1".into(), replacement.incarnation);
    assert!(control.save(&next, &rev, &mut state).unwrap());
    let (rev, mut state) = control.load().unwrap();
    assert!(
        control
            .publish(
                &next,
                &rev,
                &mut state,
                &directory,
                &scope.collection,
                &members
            )
            .unwrap()
    );
    assert_eq!(control.assignment(&scope).unwrap().phase, Phase::Active);
    assert_eq!(
        directory
            .snapshot()
            .unwrap()
            .by_scope(&scope)
            .unwrap()
            .query_routes
            .unwrap()
            .epoch,
        1
    );
    assert!(
        control
            .publish(
                &next,
                &rev,
                &mut state,
                &directory,
                &scope.collection,
                &members
            )
            .is_err()
    );
    control.revoke(&wl).unwrap();
    control.revoke(&ql).unwrap();
    control.revoke(&next.id).unwrap();
    control.revoke(&c2lease).unwrap();
}

#[test]
#[ignore = "requires isolated etcd, S3 and Kafka"]
fn retired_generation_keeps_pins_then_releases_storage_and_topic() {
    use serde_json::json;
    use vectordb_cluster::{
        directory::{Directory, Lifecycle},
        kafka::Log,
        state::{IDEMPOTENCY_WINDOW_MS, idempotency_key, now_ms},
    };
    let cluster = format!("retention-{}", uuid::Uuid::new_v4().simple());
    let directory = Directory::new("http://127.0.0.1:12379".into(), &cluster).unwrap();
    let now = now_ms();
    let request = json!({"collectionName":"items","dimension":2});
    let scope = directory
        .begin(
            "collections/create",
            &request,
            &idempotency_key(now, "create"),
        )
        .unwrap()
        .target
        .unwrap();
    directory
        .finish(&scope, Lifecycle::Active, None, 1)
        .unwrap();
    let catalog = Catalog::new("http://127.0.0.1:12379".into(), &scope).unwrap();
    let log = Log::reader("127.0.0.1:19092".into(), &scope);
    log.ensure_topic(1).unwrap();
    let objects = Objects::new(
        "http://127.0.0.1:19000".into(),
        "vectordb".into(),
        "us-east-1".into(),
        "development".into(),
        "development-only-password".into(),
    )
    .unwrap();
    let key = format!("{}/checkpoints/2-live/state.json", scope.prefix());
    objects.put(&key, vec![1]).unwrap();
    let manifest = Manifest {
        scope: scope.clone(),
        offset: 2,
        key: key.clone(),
        sha256: digest(&[1]),
        bytes: 1,
        objects: vec![],
    };
    assert!(catalog.publish(&manifest).unwrap());
    let (_, _, pin) = catalog.pin_latest().unwrap().unwrap();
    directory
        .begin(
            "collections/drop",
            &json!({"collectionName":"items"}),
            &idempotency_key(now, "drop"),
        )
        .unwrap();
    directory
        .finish(&scope, Lifecycle::Dropped, None, 3)
        .unwrap();
    directory
        .retire_dropped(now + IDEMPOTENCY_WINDOW_MS)
        .unwrap();
    assert!(directory.snapshot().unwrap().by_scope(&scope).is_ok());
    let later = now + IDEMPOTENCY_WINDOW_MS + 130_000;
    directory.retire_dropped(later).unwrap();
    assert!(directory.snapshot().unwrap().by_scope(&scope).is_err());
    assert!(catalog.acquire("retired".into()).is_err());
    directory
        .purge_retired(&objects, "127.0.0.1:19092", later + 301_000)
        .unwrap();
    assert!(objects.get(&key).is_ok());
    catalog.revoke(&pin).unwrap();
    directory
        .purge_retired(&objects, "127.0.0.1:19092", later + 301_000)
        .unwrap();
    assert!(objects.get(&key).is_err());
    assert!(catalog.manifest().unwrap().is_none());
    assert!(!catalog.publish(&manifest).unwrap());
}
