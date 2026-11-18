static ISSUED: std::sync::OnceLock<i64> = std::sync::OnceLock::new();
fn key(s: &str) -> String {
    idempotency_key(*std::sync::OnceLock::get_or_init(&ISSUED, now_ms), s)
}
fn restore(snapshot: Snapshot) -> vectordb_cluster::Result<Replica> {
    let objects: std::collections::BTreeMap<_, _> = snapshot.sealed.objects.into_iter().collect();
    Replica::restore(snapshot.checkpoint, &scope(), |key| {
        objects
            .get(key)
            .cloned()
            .ok_or_else(|| vectordb_engine::Error::Invalid("missing file".into()))
    })
}
use serde_json::{Value, json};
use vectordb_cluster::state::{
    Entry, FORMAT, Record, Replica, Scope, Snapshot, idempotency_key, now_ms,
};

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
        .prepare(replica.epoch, &key(id), operation, request)
        .unwrap()
        .unwrap();
    // Kafka transaction markers and aborted records can leave offset gaps.
    replica.apply(replica.offset + 3, record).unwrap();
    replica.result(&key(id)).unwrap().data
}
#[test]
fn uncommitted_preview_is_invisible_and_does_not_consume_ids() {
    let mut replica = replica();
    let request = json!({"collectionName":"items","data":[{"vector":[1,0]}]});
    let _uncommitted = replica
        .prepare(10, &key("lost"), "entities/insert", request.clone())
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
            .prepare(10, &key("bad"), "entities/insert", invalid)
            .is_err()
    );
    let request = json!({"collectionName":"items","data":[{"vector":[1,0]},{"vector":[0,1]}]});
    let result = write(&mut replica, "good", "entities/insert", request.clone());
    assert_eq!(result["insertIds"], json!([1, 2]));
    let recovered = restore(replica.checkpoint().unwrap()).unwrap();
    let retry = recovered
        .prepare(10, &key("good"), "entities/insert", request.clone())
        .unwrap()
        .unwrap_err();
    assert_eq!(retry.data, result);
    assert_eq!(retry.token, replica.result(&key("good")).unwrap().token);
    assert!(
        recovered
            .prepare(
                10,
                &key("good"),
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
    let snapshot = original.checkpoint().unwrap();
    let bytes = serde_json::to_vec(&snapshot.checkpoint).unwrap();
    let mut recovered = restore(Snapshot {
        checkpoint: serde_json::from_slice(&bytes).unwrap(),
        sealed: snapshot.sealed,
    })
    .unwrap();
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
            &key("insert"),
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
                &key("insert"),
                "entities/insert",
                json!({"collectionName":"items","data":[]})
            )
            .is_err()
    );
}
#[test]
fn snapshot_truncation_and_unknown_formats_are_rejected() {
    let replica = replica();
    let mut checkpoint = replica.checkpoint().unwrap();
    checkpoint.checkpoint.state.revision = checkpoint.checkpoint.offset + 1;
    assert!(restore(checkpoint).is_err());
    let mut checkpoint = replica.checkpoint().unwrap();
    checkpoint.checkpoint.format = FORMAT + 1;
    assert!(restore(checkpoint).is_err());
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
                &key("id"),
                "entities/delete",
                json!({"collectionName":"items","filter":"id > 0"})
            )
            .is_err()
    );
    let mut invalid = scope();
    invalid.generation = "../../escape".into();
    assert!(Replica::empty(invalid).is_err());
}
#[test]
fn expired_idempotency_keys_never_reexecute_after_reclamation() {
    use vectordb_cluster::state::{IDEMPOTENCY_WINDOW_MS, idempotency_key};
    let mut r = Replica::empty(scope()).unwrap();
    r.apply(
        0,
        Record {
            format: FORMAT,
            scope: scope(),
            entry: Entry::Activate { epoch: 10 },
        },
    )
    .unwrap();
    let first = idempotency_key(1_000_000, "create");
    let record = r
        .prepare_at(
            10,
            &first,
            "collections/create",
            json!({"collectionName":"items","dimension":2}),
            1_000_000,
        )
        .unwrap()
        .unwrap();
    r.apply(1, record).unwrap();
    let request = json!({"collectionName":"items","data":[{"id":1,"vector":[1,2]}]});
    let id = idempotency_key(1_000_000, "write");
    let record = r
        .prepare_at(10, &id, "entities/upsert", request.clone(), 1_000_000)
        .unwrap()
        .unwrap();
    r.apply(2, record).unwrap();
    let later = 1_000_000 + IDEMPOTENCY_WINDOW_MS;
    let new = idempotency_key(later, "write");
    let record = r
        .prepare_at(10, &new, "entities/upsert", request.clone(), later)
        .unwrap()
        .unwrap();
    r.apply(3, record).unwrap();
    assert!(r.result(&id).is_none());
    assert!(
        r.prepare_at(10, &id, "entities/upsert", request.clone(), later)
            .is_err()
    );
    let recovered = restore(r.checkpoint().unwrap()).unwrap();
    assert!(
        recovered
            .prepare_at(10, &id, "entities/upsert", request.clone(), later)
            .is_err()
    );
    assert!(
        recovered
            .prepare_at(10, &new, "entities/upsert", request, later)
            .unwrap()
            .is_err()
    );
}
#[test]
fn checkpoint_and_bounded_tail_restore_deletes_autoids_and_dedup() {
    let mut r = replica();
    write(
        &mut r,
        "first",
        "entities/insert",
        json!({"collectionName":"items","data":[{"vector":[1,2]},{"vector":[2,3]}]}),
    );
    let snapshot = r.checkpoint().unwrap();
    let deletion = r
        .prepare(
            10,
            &key("delete"),
            "entities/delete",
            json!({"collectionName":"items","ids":[1]}),
        )
        .unwrap()
        .unwrap();
    let offset = r.offset + 2;
    r.apply(offset, deletion.clone()).unwrap();
    let mut recovered = restore(snapshot).unwrap();
    recovered.apply(offset, deletion).unwrap();
    assert_eq!(
        recovered
            .query("collections/get_stats", json!({"collectionName":"items"}))
            .unwrap()["rowCount"],
        1
    );
    let data = write(
        &mut recovered,
        "next",
        "entities/insert",
        json!({"collectionName":"items","data":[{"vector":[4,5]}]}),
    );
    assert_eq!(data["insertIds"], json!([3]));
    let snapshot = recovered.checkpoint().unwrap();
    assert!(
        !serde_json::to_string(&snapshot.checkpoint)
            .unwrap()
            .contains("records")
    );
}

#[test]
fn ordered_partition_drop_survives_checkpoint_and_does_not_resurrect() {
    let mut r = replica();
    write(
        &mut r,
        "partition",
        "partitions/create",
        json!({"collectionName":"items","partitionName":"p"}),
    );
    write(
        &mut r,
        "rows",
        "entities/insert",
        json!({"collectionName":"items","partitionName":"p","data":[{"vector":[1,0]},{"vector":[0,1]},{"vector":[1,1]}]}),
    );
    write(
        &mut r,
        "already-deleted",
        "entities/delete",
        json!({"collectionName":"items","ids":[3]}),
    );
    let old = r
        .read_view("entities/count", &json!({"collectionName":"items"}))
        .unwrap();
    assert!(
        r.prepare(
            r.epoch,
            &key("bad-drop"),
            "partitions/drop",
            json!({"collectionName":"items","partitionNames":["p","missing"]})
        )
        .is_err()
    );
    write(
        &mut r,
        "drop-p",
        "partitions/drop",
        json!({"collectionName":"items","partitionNames":["p"]}),
    );
    assert_eq!(
        old.execute("entities/count", json!({"collectionName":"items"}))
            .unwrap()["count"],
        2
    );
    let mut recovered = restore(r.checkpoint().unwrap()).unwrap();
    assert_eq!(
        recovered
            .query("entities/count", json!({"collectionName":"items"}))
            .unwrap()["count"],
        0
    );
    write(
        &mut recovered,
        "recreate-p",
        "partitions/create",
        json!({"collectionName":"items","partitionName":"p"}),
    );
    assert_eq!(
        recovered
            .query(
                "entities/count",
                json!({"collectionName":"items","partitionNames":["p"]})
            )
            .unwrap()["count"],
        0
    );
    let retry = recovered
        .prepare(
            recovered.epoch,
            &key("drop-p"),
            "partitions/drop",
            json!({"collectionName":"items","partitionNames":["p"]}),
        )
        .unwrap()
        .unwrap_err();
    assert_eq!(retry.data, json!({}));
    assert_eq!(
        recovered
            .query(
                "partitions/has",
                json!({"collectionName":"items","partitionName":"p"})
            )
            .unwrap()["has"],
        true
    );
}
#[test]
fn dropped_collection_preserves_results_and_rejects_new_mutations_after_recovery() {
    let mut r = replica();
    let request = json!({"collectionName":"items","data":[{"vector":[1,0]}]});
    let result = write(&mut r, "entity", "entities/insert", request.clone());
    write(
        &mut r,
        "drop",
        "collections/drop",
        json!({"collectionName":"items"}),
    );
    let mut recovered = restore(r.checkpoint().unwrap()).unwrap();
    assert!(recovered.spec().is_none());
    assert_eq!(
        recovered
            .prepare(
                recovered.epoch,
                &key("entity"),
                "entities/insert",
                request.clone()
            )
            .unwrap()
            .unwrap_err()
            .data,
        result
    );
    assert!(
        recovered
            .prepare(recovered.epoch, &key("new"), "entities/insert", request)
            .is_err()
    );
    assert_eq!(
        recovered
            .query("collections/has", json!({"collectionName":"items"}))
            .unwrap()["has"],
        false
    );
    let mut foreign = scope();
    foreign.generation = "replacement".into();
    let record = Record {
        format: FORMAT,
        scope: foreign,
        entry: Entry::Activate { epoch: 999 },
    };
    assert!(recovered.apply(recovered.offset + 1, record).is_err());
}

#[test]
fn recurring_checkpoint_recovery_bounds_receipts_across_retention_windows() {
    let mut r = replica();
    let start = now_ms().max(r.logical_backup().clock_ms);
    for hour in 1..=96 {
        let time = start + hour * 3_600_000;
        for (op, request) in [
            (
                "entities/upsert",
                json!({"collectionName":"items","data":[{"id":1,"vector":[hour,0]},{"id":2,"vector":[0,hour]}]}),
            ),
            (
                "entities/delete",
                json!({"collectionName":"items","ids":[2]}),
            ),
        ] {
            let id = idempotency_key(time, if op.ends_with("upsert") { "up" } else { "del" });
            let record = r
                .prepare_at(r.epoch, &id, op, request, time)
                .unwrap()
                .unwrap();
            r.apply(r.offset + 2, record).unwrap();
        }
        r.reclaim();
        let snapshot = r.checkpoint().unwrap();
        assert!(snapshot.checkpoint.results.len() <= 48);
        assert!(
            snapshot
                .sealed
                .objects
                .iter()
                .map(|(_, bytes)| bytes.len())
                .sum::<usize>()
                < 32 * 1024
        );
        r = restore(snapshot).unwrap();
        assert_eq!(
            r.query("entities/count", json!({"collectionName":"items"}))
                .unwrap()["count"],
            1
        );
    }
}
