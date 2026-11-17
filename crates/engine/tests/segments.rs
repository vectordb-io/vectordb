#[path = "support/reference.rs"]
mod reference;
use reference::Reference;
use serde_json::{Value, json};
use std::collections::BTreeMap;
use vectordb_engine::{Engine, segment::SegmentEngine};
fn apply(e: &mut SegmentEngine, op: &str, r: Value) -> Value {
    let batch = e.prepare(op, r).unwrap();
    e.commit(batch, e.revision() + 1).unwrap()
}
fn native() -> SegmentEngine {
    let mut e = SegmentEngine::new();
    apply(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","dimension":2,"metricType":"L2"}),
    );
    e
}
fn roundtrip(e: &SegmentEngine) -> SegmentEngine {
    let snapshot = e.seal().unwrap();
    let objects: BTreeMap<_, _> = snapshot.objects.into_iter().collect();
    SegmentEngine::load(snapshot.manifest, |k| Ok(objects[k].clone())).unwrap()
}
#[test]
fn randomized_visibility_search_and_recovery_match_independent_model() {
    let mut e = native();
    let mut oracle = Reference::default();
    let mut random = 0x5eed_u64;
    for version in 2..602 {
        random = random.wrapping_mul(6364136223846793005).wrapping_add(1);
        let id = ((random >> 32) % 37) as i64;
        if version % 5 == 0 {
            apply(
                &mut e,
                "entities/delete",
                json!({"collectionName":"items","ids":[id]}),
            );
            oracle.put(version, id, None);
        } else {
            let vector = [(random % 101) as f64, ((random >> 16) % 127) as f64];
            apply(
                &mut e,
                "entities/upsert",
                json!({"collectionName":"items","data":[{"id":id,"vector":vector,"label":format!("v{version}"),"nullable":null}]}),
            );
            oracle.put(version, id, Some(vector));
        }
        if version % 17 == 0 {
            let before = e
                .execute(
                    "entities/search",
                    json!({"collectionName":"items","data":[[3,7]],"limit":10}),
                )
                .unwrap();
            let expected = oracle.search(version, [3., 7.], 10);
            assert_eq!(before.as_array().unwrap().len(), expected.len());
            for (hit, (id, distance)) in before.as_array().unwrap().iter().zip(expected) {
                assert_eq!(hit["id"], id);
                assert!((hit["distance"].as_f64().unwrap() - distance).abs() < 1e-9);
            }
            assert_eq!(e.row_count(), oracle.visible(version).len());
            e = roundtrip(&e);
            assert_eq!(
                before,
                e.execute(
                    "entities/search",
                    json!({"collectionName":"items","data":[[3,7]],"limit":10})
                )
                .unwrap()
            );
            e.reclaim_tombstones();
        }
    }
}
#[test]
fn native_and_sqlite_share_existing_operation_semantics() {
    let mut native = SegmentEngine::new();
    let mut legacy = Engine::memory().unwrap();
    for (op, r) in [
        (
            "collections/create",
            json!({"collectionName":"items","dimension":2,"autoID":true,"metricType":"L2"}),
        ),
        (
            "partitions/create",
            json!({"collectionName":"items","partitionName":"p"}),
        ),
        (
            "entities/insert",
            json!({"collectionName":"items","partitionName":"p","data":[{"vector":[1.0,2.0],"label":"a"},{"vector":[2.0,4.0],"label":"b"}]}),
        ),
        (
            "entities/upsert",
            json!({"collectionName":"items","data":[{"id":1,"vector":[8.0,2.0],"label":"c"}]}),
        ),
    ] {
        assert_eq!(
            apply(&mut native, op, r.clone()),
            legacy.execute(op, r).unwrap()
        );
    }
    for (op, r) in [
        (
            "entities/query",
            json!({"collectionName":"items","filter":"id > 0","outputFields":["*"],"orderByFields":["id:desc"]}),
        ),
        (
            "entities/get",
            json!({"collectionName":"items","id":[1,2],"outputFields":["*"]}),
        ),
        (
            "entities/search",
            json!({"collectionName":"items","data":[[1,1]],"outputFields":["label"],"limit":2}),
        ),
        (
            "partitions/get_stats",
            json!({"collectionName":"items","partitionName":"p"}),
        ),
    ] {
        assert_eq!(
            native.execute(op, r.clone()).unwrap(),
            legacy.execute(op, r).unwrap()
        );
    }
}
#[test]
fn staged_batches_are_atomic_stale_batches_rejected_and_dtype_explicit() {
    let mut e = native();
    let bad =
        json!({"collectionName":"items","data":[{"id":1,"vector":[1,2]},{"id":2,"vector":[1]}]});
    assert!(e.prepare("entities/insert", bad).is_err());
    assert_eq!(e.row_count(), 0);
    let batch = e
        .prepare(
            "entities/insert",
            json!({"collectionName":"items","data":[{"id":1,"vector":[1.23456789123,2]}]}),
        )
        .unwrap();
    let stale = e
        .prepare(
            "entities/insert",
            json!({"collectionName":"items","data":[{"id":2,"vector":[1,2]}]}),
        )
        .unwrap();
    e.commit(batch, 2).unwrap();
    assert!(e.commit(stale, 3).is_err());
    assert_eq!(
        e.get_version(&json!(1)).unwrap().body.unwrap()["vector"][0],
        json!((1.23456789123_f64 as f32) as f64)
    );
    let mut f64engine = SegmentEngine::new();
    apply(
        &mut f64engine,
        "collections/create",
        json!({"collectionName":"items","dimension":2,"vectorDtype":"Float64"}),
    );
    apply(
        &mut f64engine,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[1.23456789123,2]}]}),
    );
    assert_eq!(
        roundtrip(&f64engine)
            .get_version(&json!(1))
            .unwrap()
            .body
            .unwrap()["vector"][0],
        json!(1.23456789123)
    );
    assert!(
        e.prepare(
            "entities/upsert",
            json!({"collectionName":"items","data":[{"id":1,"vector":[1e100,2]}]})
        )
        .is_err()
    );
}
#[test]
fn corruption_missing_objects_and_invalid_manifests_fail_closed() {
    let mut e = native();
    apply(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[1,2]}]}),
    );
    let sealed = e.seal().unwrap();
    let objects: BTreeMap<_, _> = sealed.objects.into_iter().collect();
    for missing in objects.keys() {
        assert!(
            SegmentEngine::load(sealed.manifest.clone(), |k| if k == missing {
                Err(vectordb_engine::Error::Invalid("missing".into()))
            } else {
                Ok(objects[k].clone())
            })
            .is_err()
        );
    }
    for corrupt in objects.keys() {
        assert!(
            SegmentEngine::load(sealed.manifest.clone(), |k| {
                let mut b = objects[k].clone();
                if k == corrupt {
                    b[0] ^= 0xff;
                }
                Ok(b)
            })
            .is_err()
        );
    }
    let mut bad = sealed.manifest.clone();
    bad.segments[0].rows += 1;
    assert!(SegmentEngine::load(bad, |k| Ok(objects[k].clone())).is_err());
    let mut bad = sealed.manifest.clone();
    bad.spec.as_mut().unwrap().dtype = vectordb_engine::segment::VectorDtype::Float64;
    assert!(SegmentEngine::load(bad, |k| Ok(objects[k].clone())).is_err());
    let mut bad = sealed.manifest.clone();
    bad.segments.push(bad.segments[0].clone());
    assert!(SegmentEngine::load(bad, |k| Ok(objects[k].clone())).is_err());
}
#[test]
fn repeated_updates_have_bounded_state_and_deleted_rows_do_not_resurrect() {
    let mut e = native();
    let mut baseline = 0;
    for version in 0..2000 {
        apply(
            &mut e,
            "entities/upsert",
            json!({"collectionName":"items","data":[{"id":1,"vector":[1,2]}]}),
        );
        if version == 0 {
            baseline = e.state_bytes();
        }
    }
    assert_eq!(e.state_bytes(), baseline);
    let size = e
        .seal()
        .unwrap()
        .objects
        .iter()
        .map(|(_, b)| b.len())
        .sum::<usize>();
    assert!(size < 16 * 1024);
    apply(
        &mut e,
        "entities/delete",
        json!({"collectionName":"items","ids":[1,999]}),
    );
    let mut restored = roundtrip(&e);
    assert_eq!(restored.row_count(), 0);
    assert!(restored.get_version(&json!(1)).unwrap().body.is_none());
    restored.reclaim_tombstones();
    assert!(restored.get_version(&json!(1)).is_none());
    assert_eq!(roundtrip(&restored).row_count(), 0);
}
#[test]
fn sparse_binary_nullable_dynamic_and_multiple_segments_roundtrip() {
    let mut e = SegmentEngine::new();
    apply(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","schema":{"fields":[{"fieldName":"id","dataType":"Int64","isPrimary":true},{"fieldName":"b","dataType":"BinaryVector","elementTypeParams":{"dim":8}},{"fieldName":"s","dataType":"SparseFloatVector"},{"fieldName":"n","dataType":"Int64","nullable":true}],"enableDynamicField":true}}),
    );
    let data = (0..4200)
        .map(|i| json!({"id":i,"b":[i%256],"s":{"3":0.25,"7":0.5},"extra":{"a":[1,null]}}))
        .collect::<Vec<_>>();
    apply(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":data}),
    );
    assert_eq!(e.seal().unwrap().manifest.segments.len(), 2);
    let restored = roundtrip(&e);
    assert_eq!(restored.row_count(), 4200);
    assert_eq!(
        serde_json::to_value(restored.get_version(&json!(4199))).unwrap(),
        serde_json::to_value(e.get_version(&json!(4199))).unwrap()
    );
}
#[test]
fn prepared_batch_survives_safe_tombstone_reclamation_without_stale_accounting() {
    let mut e = native();
    apply(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[1,2]}]}),
    );
    apply(
        &mut e,
        "entities/delete",
        json!({"collectionName":"items","ids":[1]}),
    );
    let prepared = e
        .prepare(
            "entities/upsert",
            json!({"collectionName":"items","data":[{"id":2,"vector":[3,4]}]}),
        )
        .unwrap();
    let _persisted = roundtrip(&e);
    e.reclaim_tombstones();
    e.commit(prepared, e.revision() + 1).unwrap();
    assert_eq!(e.state_bytes(), roundtrip(&e).state_bytes());
}
#[test]
fn query_dtype_matches_storage_and_float_overflow_is_explicit() {
    let mut e = native();
    apply(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[1.23456789123,2]}]}),
    );
    let exact = e
        .execute(
            "entities/search",
            json!({"collectionName":"items","data":[[1.23456789123,2]],"limit":1}),
        )
        .unwrap();
    assert_eq!(exact[0]["distance"], 0.0);
    assert!(
        e.execute(
            "entities/search",
            json!({"collectionName":"items","data":[[1e100,2]]})
        )
        .is_err()
    );
}
