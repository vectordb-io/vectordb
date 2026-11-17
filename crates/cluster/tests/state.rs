use serde_json::{Value, json};
use vectordb_cluster::state::{Entry, FORMAT, Record, Replica, Scope};

fn scope() -> Scope {
    Scope {
        cluster: "test".into(),
        collection: "items".into(),
        generation: "g1".into(),
    }
}
fn replica() -> Replica {
    let mut replica = Replica::empty(scope()).unwrap();
    replica
        .apply(
            0,
            Record {
                format: FORMAT,
                scope: scope(),
                entry: Entry::Activate { epoch: 10 },
            },
        )
        .unwrap();
    write(
        &mut replica,
        "create",
        "collections/create",
        json!({"collectionName":"items","dimension":2,"autoID":true}),
    );
    replica
}
fn write(replica: &mut Replica, id: &str, operation: &str, request: Value) -> Value {
    let record = replica
        .prepare(replica.epoch, id, operation, request)
        .unwrap()
        .unwrap();
    // Kafka transaction markers and aborted records can leave offset gaps.
    replica.apply(replica.offset + 3, record).unwrap();
    replica.result(id).unwrap().data
}
#[test]
fn uncommitted_preview_is_invisible_and_does_not_consume_ids() {
    let mut replica = replica();
    let request = json!({"collectionName":"items","data":[{"vector":[1,0]}]});
    let _uncommitted = replica
        .prepare(10, "lost", "entities/insert", request.clone())
        .unwrap()
        .unwrap();
    assert_eq!(
        replica
            .query("collections/get_stats", json!({"collectionName":"items"}))
            .unwrap()["rowCount"],
        0
    );
    let result = write(&mut replica, "accepted", "entities/insert", request);
    assert_eq!(result["insertIds"], json!([1]));
}
#[test]
fn failed_batch_rolls_back_and_retry_recovers_original_ids() {
    let mut replica = replica();
    let invalid = json!({"collectionName":"items","data":[{"vector":[1,0]},{"vector":[1]}]});
    assert!(
        replica
            .prepare(10, "bad", "entities/insert", invalid)
            .is_err()
    );
    let request = json!({"collectionName":"items","data":[{"vector":[1,0]},{"vector":[0,1]}]});
    let result = write(&mut replica, "good", "entities/insert", request.clone());
    assert_eq!(result["insertIds"], json!([1, 2]));
    let recovered = Replica::restore(replica.checkpoint(), &scope()).unwrap();
    let retry = recovered
        .prepare(10, "good", "entities/insert", request.clone())
        .unwrap()
        .unwrap_err();
    assert_eq!(retry.data, result);
    assert_eq!(retry.token, replica.result("good").unwrap().token);
    assert!(
        recovered
            .prepare(
                10,
                "good",
                "entities/insert",
                json!({"collectionName":"items","data":[{"vector":[9,9]}]})
            )
            .is_err()
    );
}
#[test]
fn checkpoint_replay_preserves_search_and_partition_state() {
    let mut original = replica();
    write(
        &mut original,
        "partition",
        "partitions/create",
        json!({"collectionName":"items","partitionName":"other"}),
    );
    write(
        &mut original,
        "insert",
        "entities/insert",
        json!({"collectionName":"items","partitionName":"other","data":[{"vector":[1,0]},{"vector":[0,1]}]}),
    );
    let bytes = serde_json::to_vec(&original.checkpoint()).unwrap();
    let mut recovered =
        Replica::restore(serde_json::from_slice(&bytes).unwrap(), &scope()).unwrap();
    let request = json!({"collectionName":"items","data":[[1,0]],"limit":2});
    assert_eq!(
        original.query("entities/search", request.clone()).unwrap(),
        recovered.query("entities/search", request).unwrap()
    );
    assert_eq!(original.offset, recovered.offset);
}
#[test]
fn stale_epoch_foreign_scope_and_corruption_fail_closed() {
    let mut replica = replica();
    let old_offset = replica.offset;
    assert!(
        replica
            .apply(
                old_offset,
                Record {
                    format: FORMAT,
                    scope: scope(),
                    entry: Entry::Barrier { epoch: 10 }
                }
            )
            .is_err()
    );
    assert!(
        replica
            .apply(
                old_offset + 1,
                Record {
                    format: FORMAT,
                    scope: scope(),
                    entry: Entry::Barrier { epoch: 9 }
                }
            )
            .is_err()
    );
    let mut foreign = scope();
    foreign.generation = "g2".into();
    assert!(
        replica
            .apply(
                old_offset + 1,
                Record {
                    format: FORMAT,
                    scope: foreign,
                    entry: Entry::Barrier { epoch: 10 }
                }
            )
            .is_err()
    );
    let mut prepared = replica
        .prepare(
            10,
            "insert",
            "entities/insert",
            json!({"collectionName":"items","data":[{"vector":[1,0]}]}),
        )
        .unwrap()
        .unwrap();
    if let Entry::Mutation { result, .. } = &mut prepared.entry {
        *result = json!({"insertIds":[999]});
    }
    assert!(replica.apply(old_offset + 1, prepared).is_err());
    assert_eq!(replica.offset, old_offset);
    assert_eq!(
        replica
            .query("collections/get_stats", json!({"collectionName":"items"}))
            .unwrap()["rowCount"],
        0
    );
    replica
        .apply(
            old_offset + 1,
            Record {
                format: FORMAT,
                scope: scope(),
                entry: Entry::Activate { epoch: 11 },
            },
        )
        .unwrap();
    assert!(
        replica
            .prepare(
                10,
                "insert",
                "entities/insert",
                json!({"collectionName":"items","data":[]})
            )
            .is_err()
    );
}
#[test]
fn snapshot_truncation_and_unknown_formats_are_rejected() {
    let replica = replica();
    let mut checkpoint = replica.checkpoint();
    checkpoint.records.pop();
    assert!(Replica::restore(checkpoint, &scope()).is_err());
    let mut checkpoint = replica.checkpoint();
    checkpoint.format = FORMAT + 1;
    assert!(Replica::restore(checkpoint, &scope()).is_err());
}
#[test]
fn read_path_cannot_mutate_and_scope_cannot_escape() {
    let mut replica = replica();
    assert!(
        replica
            .query("collections/drop", json!({"collectionName":"items"}))
            .is_err()
    );
    assert!(
        replica
            .query("entities/get", json!({"collectionName":"different","id":1}))
            .is_err()
    );
    assert!(
        replica
            .prepare(
                10,
                "id",
                "entities/delete",
                json!({"collectionName":"items","filter":"id > 0"})
            )
            .is_err()
    );
    let mut invalid = scope();
    invalid.generation = "../../escape".into();
    assert!(Replica::empty(invalid).is_err());
}
