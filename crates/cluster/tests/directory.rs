use serde_json::{Value, json};
use vectordb_cluster::directory::{DirectoryState, Lifecycle, receipt_name};
fn plan(state: &mut DirectoryState, op: &str, r: Value) -> vectordb_cluster::directory::Receipt {
    state.plan("test", op, &r, "request").unwrap()
}
fn create(state: &mut DirectoryState, db: &str, name: &str) -> vectordb_cluster::state::Scope {
    let receipt = plan(
        state,
        "collections/create",
        json!({"dbName":db,"collectionName":name,"dimension":2}),
    );
    let scope = receipt.target.unwrap();
    assert_eq!(
        state.collections[&scope.collection].state,
        Lifecycle::Creating
    );
    assert!(state.resolve(db, name).is_err());
    state.collections.get_mut(&scope.collection).unwrap().state = Lifecycle::Active;
    scope
}
#[test]
fn catalog_lifecycle_roundtrips_and_recreation_never_reuses_identity() {
    let mut state = DirectoryState::default();
    let old = create(&mut state, "default", "items");
    let old_spec = state.collections[&old.collection].spec.clone().unwrap();
    assert_eq!(old_spec.schema.fields.len(), 2);
    plan(
        &mut state,
        "collections/drop",
        json!({"collectionName":"items"}),
    );
    assert_eq!(
        state.collections[&old.collection].state,
        Lifecycle::Dropping
    );
    let mut restored: DirectoryState =
        serde_json::from_slice(&serde_json::to_vec(&state).unwrap()).unwrap();
    assert!(restored.resolve("default", "items").is_err());
    assert!(
        restored
            .plan(
                "test",
                "collections/create",
                &json!({"collectionName":"items","dimension":2}),
                "another"
            )
            .is_err()
    );
    restored.collections.get_mut(&old.collection).unwrap().state = Lifecycle::Dropped;
    let new = create(&mut restored, "default", "items");
    assert_ne!(old, new);
    assert_eq!(restored.resolve("default", "items").unwrap().scope, new);
    assert_eq!(restored.by_scope(&old).unwrap().state, Lifecycle::Dropped);
    assert_ne!(old.topic(), new.topic());
}
#[test]
fn aliases_pin_identity_and_databases_isolate_names() {
    let mut state = DirectoryState::default();
    plan(&mut state, "databases/create", json!({"dbName":"other"}));
    let first = create(&mut state, "default", "a");
    let second = create(&mut state, "default", "b");
    let other = create(&mut state, "other", "a");
    assert_ne!(first, other);
    plan(
        &mut state,
        "aliases/create",
        json!({"aliasName":"live","collectionName":"a"}),
    );
    let resolved = state.resolve("default", "live").unwrap();
    let receipt = plan(
        &mut state,
        "entities/upsert",
        json!({"collectionName":"live","data":[]}),
    );
    plan(
        &mut state,
        "aliases/alter",
        json!({"aliasName":"live","collectionName":"b"}),
    );
    assert_eq!(receipt.target.unwrap(), first);
    assert_eq!(resolved.scope, first);
    assert_eq!(state.resolve("default", "live").unwrap().scope, second);
    assert!(
        state
            .plan("test", "databases/drop", &json!({"dbName":"other"}), "x")
            .is_err()
    );
    plan(
        &mut state,
        "collections/drop",
        json!({"collectionName":"b"}),
    );
    assert!(state.resolve("default", "live").is_err());
    assert_eq!(state.resolve("other", "a").unwrap().scope, other);
}
#[test]
fn catalog_rejects_invalid_definitions_and_receipt_keys_are_scoped() {
    let mut state = DirectoryState::default();
    for request in [
        json!({"collectionName":"bad","dimension":0}),
        json!({"collectionName":"bad","dimension":2,"typo":true}),
        json!({"dbName":"absent","collectionName":"bad","dimension":2}),
    ] {
        assert!(
            state
                .plan("test", "collections/create", &request, "id")
                .is_err()
        );
        assert!(state.collections.is_empty());
    }
    let one = json!({"collectionName":"a"});
    assert_eq!(
        receipt_name(&one, "v2-1000-key").unwrap(),
        receipt_name(
            &json!({"dbName":"default","collectionName":"a"}),
            "v2-1000-key"
        )
        .unwrap()
    );
    assert_ne!(
        receipt_name(&one, "v2-1000-key").unwrap(),
        receipt_name(
            &json!({"dbName":"other","collectionName":"a"}),
            "v2-1000-key"
        )
        .unwrap()
    );
    assert!(
        state
            .plan("test", "databases/drop", &json!({"dbName":"default"}), "id")
            .is_err()
    );
}
