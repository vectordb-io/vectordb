use serde_json::json;
use vectordb_engine::{Database, LocalStore};

#[test]
fn local_wal_checkpoint_and_backup_preserve_native_query_results() {
    assert!(matches!(
        Database::open(":memory:").unwrap(),
        Database::SQLite(_)
    ));
    let temp = tempfile::tempdir().unwrap();
    let root = temp.path().join("native.vdb");
    let mut db = Database::open(&root).unwrap();
    assert!(LocalStore::open(&root).is_err());
    let base = json!({"dbName":"test","collectionName":"items"});
    db.execute("databases/create", json!({"dbName":"test"}))
        .unwrap();
    db.execute(
        "collections/create",
        json!({"dbName":"test","collectionName":"items","dimension":2,"autoID":true}),
    )
    .unwrap();
    db.execute(
        "aliases/create",
        json!({"dbName":"test","collectionName":"items","aliasName":"current"}),
    )
    .unwrap();
    db.execute(
        "partitions/create",
        json!({"dbName":"test","collectionName":"items","partitionName":"p"}),
    )
    .unwrap();
    let insert = json!({"dbName":"test","collectionName":"current","partitionName":"p","data":[{"vector":[1,0]}]});
    for _ in 0..130 {
        db.execute("entities/insert", insert.clone()).unwrap();
    }
    db.execute(
        "entities/delete",
        json!({"dbName":"test","collectionName":"current","ids":[130]}),
    )
    .unwrap();
    drop(db);
    let mut db = Database::open(&root).unwrap();
    assert_eq!(
        db.execute("entities/count", base.clone()).unwrap()["count"],
        129
    );
    assert_eq!(
        db.execute("entities/insert", insert).unwrap()["insertIds"],
        json!([131])
    );
    db.flush().unwrap();
    assert_eq!(std::fs::read_dir(root.join("wal")).unwrap().count(), 0);
    let snapshot = temp.path().join("copy");
    db.backup(&snapshot).unwrap();
    db.execute("collections/drop", base.clone()).unwrap();
    let mut restored = Database::open(&snapshot).unwrap();
    assert_eq!(
        restored.execute("entities/count", base).unwrap()["count"],
        130
    );
    assert_eq!(
        restored
            .execute(
                "aliases/list",
                json!({"dbName":"test","collectionName":"items"})
            )
            .unwrap(),
        json!(["current"])
    );
    let mut bytes = vec![];
    assert_eq!(
        restored
            .export_jsonl("test", "current", &mut bytes)
            .unwrap(),
        130
    );
    drop(restored);
    std::fs::write(snapshot.join("HEAD"), b"{}").unwrap();
    assert!(Database::open(snapshot).is_err());
}
