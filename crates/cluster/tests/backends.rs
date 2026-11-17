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
    let key = format!("{}/checkpoints/object.json", scope.prefix());
    let bytes = b"immutable test object".to_vec();
    objects.put(&key, bytes.clone()).unwrap();
    assert_eq!(objects.get(&key).unwrap(), bytes);
    assert!(objects.put(&key, b"overwrite".to_vec()).is_err());
    assert_eq!(objects.get(&key).unwrap(), bytes);
    assert!(objects.get("../escape").is_err());
    let mut manifest = Manifest {
        scope,
        offset: 10,
        key,
        sha256: digest(&bytes),
        bytes: bytes.len(),
    };
    assert!(catalog.publish(&manifest).unwrap());
    manifest.offset = 9;
    assert!(!catalog.publish(&manifest).unwrap());
    assert_eq!(catalog.manifest().unwrap().unwrap().offset, 10);
    manifest.offset = 11;
    assert!(catalog.publish(&manifest).unwrap());
    catalog.revoke(&lease.id).unwrap();
    assert!(catalog.verify(&lease).is_err());
    let next = catalog.acquire("http://127.0.0.1:19532".into()).unwrap();
    assert!(next.epoch > lease.epoch);
    assert!(catalog.verify(&lease).is_err());
    catalog.revoke(&next.id).unwrap();
}
