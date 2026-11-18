use super::*;
use serde::{Deserialize, Serialize};
use std::{collections::BTreeMap, fs, io::Write, path::Path as FsPath};
use vectordb_cluster::{
    directory::{Directory, DirectoryState, Lifecycle, Receipt},
    state::LogicalBackup,
};

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Archive {
    format: u32,
    cluster: String,
    directory: DirectoryState,
    receipts: BTreeMap<String, Receipt>,
    // Key is the physical collection ID, value is SHA-256 of its logical snapshot.
    collections: BTreeMap<String, String>,
}

fn write_new(path: &FsPath, bytes: &[u8]) -> Result<()> {
    let mut f = fs::File::create_new(path)?;
    f.write_all(bytes)?;
    f.sync_all()?;
    Ok(())
}

pub(super) fn run(config: Config) -> Result<()> {
    let path = config
        .archive
        .as_deref()
        .ok_or_else(|| Error::Invalid("--archive required".into()))?;
    let directory = Directory::new(config.etcd.clone(), &config.cluster)?;
    let objects = Objects::new(
        config.s3_endpoint.clone(),
        config.bucket.clone(),
        config.region.clone(),
        config.access_key.clone(),
        config.secret_key.clone(),
    )?;
    let store = directory.maintenance_store();
    let lease = store.acquire("database-maintenance".into())?;
    let (done, wait) = std::sync::mpsc::channel::<()>();
    let renew = store.clone();
    let guard = lease.clone();
    let thread = std::thread::spawn(move || {
        while matches!(
            wait.recv_timeout(Duration::from_secs(5)),
            Err(std::sync::mpsc::RecvTimeoutError::Timeout)
        ) {
            if renew.renew(&guard).is_err() {
                break;
            }
        }
    });
    let result = match config.role {
        Role::Backup => backup(&config, path, &directory, &objects, &lease),
        Role::Restore => restore(&config, path, &directory, &objects, &lease),
        _ => unreachable!(),
    };
    drop(done);
    let _ = thread.join();
    let _ = store.revoke(&lease.id);
    result
}

fn backup(
    config: &Config,
    path: &FsPath,
    directory: &Directory,
    objects: &Objects,
    lease: &Lease,
) -> Result<()> {
    let gateway = advertised_origin(config.gateway.as_deref())?;
    let client = vectordb_cluster::client::Client::new(&gateway, &config.token)?;
    let mut state = directory.snapshot()?;
    if state
        .collections
        .values()
        .any(|c| matches!(c.state, Lifecycle::Creating | Lifecycle::Dropping))
    {
        return Err(Error::Conflict(
            "retry backup after pending catalog transitions complete".into(),
        ));
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::DirBuilderExt;
        fs::DirBuilder::new().mode(0o700).create(path)?;
    }
    #[cfg(not(unix))]
    fs::create_dir(path)?;
    let mut hashes = BTreeMap::new();
    for (id, c) in &mut state.collections {
        c.query_routes = None;
        let catalog = Catalog::new(config.etcd.clone(), &c.scope)?;
        if c.state == Lifecycle::Active {
            let result = client.execute(
                "collections/flush",
                json!({"dbName":c.database,"collectionName":c.name}),
                None,
            )?;
            let scope: Scope = serde_json::from_value(result["data"]["manifest"]["scope"].clone())?;
            if scope != c.scope {
                return Err(Error::Conflict(
                    "gateway belongs to a different collection or cluster".into(),
                ));
            }
        }
        let mut replica = Runtime::load_checkpoint(&catalog, objects, &c.scope)?
            .unwrap_or(Replica::empty(c.scope.clone())?);
        if c.state == Lifecycle::Dropped {
            Log::reader(config.kafka.clone(), &c.scope)
                .replay_tail(replica.offset, |n, r| replica.apply(n, r))?;
        }
        if (c.state == Lifecycle::Active) != replica.spec().is_some() {
            return Err(Error::Integrity(
                "catalog and checkpoint lifecycle differ".into(),
            ));
        }
        c.spec = replica.spec().cloned();
        let bytes = serde_json::to_vec(&replica.logical_backup())?;
        write_new(&path.join(format!("{id}.json")), &bytes)?;
        hashes.insert(id.clone(), digest(&bytes));
    }
    let mut receipts = directory.backup_receipts()?;
    receipts.retain(|_, r| {
        r.target
            .as_ref()
            .is_none_or(|s| state.collections.contains_key(&s.collection))
    });
    let manifest = Archive {
        format: 1,
        cluster: config.cluster.clone(),
        directory: state,
        receipts,
        collections: hashes,
    };
    let bytes = serde_json::to_vec(&manifest)?;
    write_new(&path.join("catalog.json"), &bytes)?;
    directory.maintenance_store().verify(lease)?;
    write_new(&path.join("COMPLETE"), digest(&bytes).as_bytes())?;
    fs::File::open(path)?.sync_all()?;
    println!(
        "{}",
        json!({"collections":manifest.collections.len(),"catalogSha256":digest(&bytes)})
    );
    Ok(())
}

fn read_collection(path: &FsPath, id: &str, hash: &str) -> Result<LogicalBackup> {
    vectordb_cluster::identifier(id)?;
    let bytes = fs::read(path.join(format!("{id}.json")))?;
    if digest(&bytes) != hash {
        return Err(Error::Integrity("backup collection digest mismatch".into()));
    }
    Ok(serde_json::from_slice(&bytes)?)
}
fn restore(
    config: &Config,
    path: &FsPath,
    directory: &Directory,
    objects: &Objects,
    lease: &Lease,
) -> Result<()> {
    let bytes = fs::read(path.join("catalog.json"))?;
    let hash = digest(&bytes);
    if fs::read(path.join("COMPLETE"))? != hash.as_bytes() {
        return Err(Error::Integrity(
            "incomplete backup or catalog digest mismatch".into(),
        ));
    }
    let mut archive: Archive = serde_json::from_slice(&bytes)?;
    if archive.format != 1
        || archive.directory.format != 1
        || archive.cluster == config.cluster
        || archive
            .collections
            .keys()
            .ne(archive.directory.collections.keys())
    {
        return Err(Error::Invalid(
            "restore requires supported backup and a new cluster ID".into(),
        ));
    }
    if !(1..=5).contains(&config.wal_replicas) {
        return Err(Error::Invalid("WAL replicas must be 1..5".into()));
    }
    let catalog = &archive.directory;
    if !catalog.databases.contains("default")
        || catalog.databases.len() > 32
        || catalog.collections.len() > 256
        || catalog
            .collections
            .values()
            .filter(|c| c.state == Lifecycle::Active)
            .count()
            > vectordb_cluster::directory::MAX_COLLECTIONS
        || catalog
            .collections
            .values()
            .any(|c| c.state == Lifecycle::Active && !catalog.databases.contains(&c.database))
        || catalog.aliases.iter().any(|(alias, id)| {
            !catalog.collections.get(id).is_some_and(|c| {
                c.state == Lifecycle::Active
                    && alias.split_once('/').is_some_and(|(db, name)| {
                        db == c.database && vectordb_cluster::identifier(name).is_ok()
                    })
            })
        })
    {
        return Err(Error::Integrity(
            "invalid backup catalog or capacity exceeded".into(),
        ));
    }
    for (id, sum) in &archive.collections {
        let b = read_collection(path, id, sum)?;
        let c = &archive.directory.collections[id];
        if b.scope != c.scope
            || b.scope.cluster != archive.cluster
            || b.scope.collection != *id
            || (c.state == Lifecycle::Active) != b.spec.is_some()
            || !matches!(c.state, Lifecycle::Active | Lifecycle::Dropped)
        {
            return Err(Error::Integrity(
                "backup catalog identity or lifecycle mismatch".into(),
            ));
        }
        // Validate logical rows before reserving the target; never hold two collections at once.
        vectordb_engine::segment::SegmentEngine::from_backup(b.spec, b.rows, 1)?;
    }
    directory.restore_step(lease, &hash, &[], None)?;
    for (id, sum) in &archive.collections {
        directory.maintenance_store().verify(lease)?;
        let backup = read_collection(path, id, sum)?;
        let c = archive.directory.collections.get_mut(id).unwrap();
        c.scope.cluster = config.cluster.clone();
        // A paused, fenced restore can only write to its own invisible generation.
        c.scope.generation = uuid::Uuid::new_v4().simple().to_string();
        c.query_routes = None;
        directory.stage_restore(lease, &c.scope)?;
        let mut log = Log::reader(config.kafka.clone(), &c.scope);
        log.ensure_topic(config.wal_replicas)?;
        if let Some(n) = c.write_channels {
            log.participants.ensure(&c.scope, n, config.wal_replicas)?;
        }
        log.initialize_writer()?;
        for entry in [Entry::Activate { epoch: 1 }, Entry::Barrier { epoch: 1 }] {
            c.spec_offset = log.append(&Record {
                format: FORMAT,
                scope: c.scope.clone(),
                entry,
            })?;
        }
        let (snapshot, expected_digest) =
            Replica::bootstrap(backup, c.scope.clone(), c.spec_offset, 1)?;
        c.spec = snapshot.checkpoint.state.spec.clone();
        let catalog = Catalog::new(config.etcd.clone(), &c.scope)?;
        let manifest = upload_snapshot(objects, snapshot)?;
        if !catalog.publish(&manifest)? {
            return Err(Error::Conflict(
                "bootstrap checkpoint publication failed".into(),
            ));
        }
        // Re-open from shared storage before publishing any collection names.
        let verified = Runtime::load_checkpoint(&catalog, objects, &c.scope)?
            .ok_or(Error::CheckpointRequired)?;
        if verified.offset != c.spec_offset || verified.content_digest()? != expected_digest {
            return Err(Error::Integrity(
                "restored content digest or watermark mismatch".into(),
            ));
        }
        log.trim(c.spec_offset)?;
    }
    node::failpoint("restore_staged");
    for receipt in archive.receipts.values_mut() {
        if let Some(scope) = &mut receipt.target {
            *scope = archive
                .directory
                .collections
                .get(&scope.collection)
                .ok_or_else(|| Error::Integrity("missing receipt target".into()))?
                .scope
                .clone();
        }
    }
    let receipts = archive.receipts.into_iter().collect::<Vec<_>>();
    for batch in receipts.chunks(32) {
        directory.restore_step(lease, &hash, batch, None)?;
    }
    directory.restore_step(lease, &hash, &[], Some(&archive.directory))?;
    println!(
        "{}",
        json!({"restoredCollections":archive.collections.len(),"sourceCatalogSha256":hash,"cluster":config.cluster})
    );
    Ok(())
}
