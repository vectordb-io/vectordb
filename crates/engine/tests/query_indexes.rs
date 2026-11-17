use serde_json::{Value, json};
use std::{
    collections::BTreeMap,
    sync::{Arc, Barrier},
    thread,
};
use vectordb_engine::{query_index::Budget, segment::SegmentEngine, segment_storage::checksum};
fn apply(e: &mut SegmentEngine, op: &str, r: Value) {
    let b = e.prepare(op, r).unwrap();
    e.commit(b, e.revision() + 1).unwrap();
}
fn data(n: usize) -> Vec<Value> {
    (0..n).map(|i|json!({"id":i,"tag":i%20,"rating":(i%17) as f64,"vector":[(i%29) as f64,(i%31) as f64,(i%37) as f64,(i%41) as f64]})).collect()
}
fn create(kind: &str) -> Value {
    json!({"collectionName":"items","schema":{"fields":[{"fieldName":"id","dataType":"Int64","isPrimary":true},{"fieldName":"tag","dataType":"Int64","nullable":true},{"fieldName":"rating","dataType":"Double"},{"fieldName":"vector","dataType":"FloatVector","elementTypeParams":{"dim":4}}]},"indexParams":[{"fieldName":"vector","indexType":kind,"metricType":"L2","params":match kind{"HNSW"=>json!({"M":8,"efConstruction":64}),"IVF_FLAT"=>json!({"nlist":8}),_=>json!({})}}]})
}
fn engine(kind: &str, n: usize) -> SegmentEngine {
    let mut e = SegmentEngine::new();
    apply(&mut e, "collections/create", create(kind));
    apply(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":data(n)}),
    );
    e
}
fn search(e: &SegmentEngine, filter: &str) -> Value {
    e.execute(
        "entities/search",
        json!({"collectionName":"items","data":[[3,4,5,6]],"limit":5,"filter":filter}),
    )
    .unwrap()
}
fn restore(e: &SegmentEngine) -> SegmentEngine {
    let s = e.seal().unwrap();
    let objects: BTreeMap<_, _> = s.objects.into_iter().collect();
    SegmentEngine::load(s.manifest, |k| Ok(objects[k].clone())).unwrap()
}
#[test]
fn both_ann_types_persist_reload_and_do_not_rebuild_on_queries_or_one_write() {
    for kind in ["HNSW", "IVF_FLAT"] {
        let mut e = engine(kind, 2048);
        e.build_indexes().unwrap();
        let expected = search(&e, "");
        let before = e.index_status()["builtSegments"].as_u64().unwrap();
        assert_eq!(before, 32);
        let mut loaded = restore(&e);
        assert_eq!(loaded.index_status()["loadedSegments"], 32);
        assert_eq!(loaded.index_status()["builtSegments"], 0);
        assert_eq!(search(&loaded, ""), expected);
        let update = json!({"collectionName":"items","data":[{"id":0,"tag":0,"rating":1,"vector":[3,4,5,6]}]});
        apply(&mut e, "entities/upsert", update.clone());
        apply(&mut loaded, "entities/upsert", update);
        assert!(
            e.index_jobs().is_empty(),
            "one update must stay in the exact delta"
        );
        assert_eq!(search(&e, "")[0]["id"], 0);
        assert_eq!(search(&loaded, ""), search(&e, ""));
        assert_eq!(e.index_status()["builtSegments"], before);
        let again = restore(&e);
        assert_eq!(search(&again, ""), search(&e, ""));
    }
}
#[test]
fn strict_filter_deletes_updates_and_scalar_masks_match_exact_search() {
    let mut flat = engine("FLAT", 2048);
    let mut ann = engine("IVF_FLAT", 2048);
    ann.build_indexes().unwrap();
    for e in [&mut flat, &mut ann] {
        apply(
            e,
            "entities/delete",
            json!({"collectionName":"items","ids":[3,4,5,6,7]}),
        );
        apply(
            e,
            "entities/upsert",
            json!({"collectionName":"items","data":[{"id":0,"tag":null,"rating":3.5,"vector":[1,1,1,1]},{"id":2050,"tag":19,"rating":9,"vector":[3,4,5,6]}]}),
        );
    }
    for filter in [
        "id >= 1000 and id < 1020",
        "tag == 19 and rating >= 7",
        "tag in [1,2] and rating < 2",
        "tag is null",
        "not (tag == 3)",
        "id > 999999",
        "tag == 19 or id == 2050",
    ] {
        let query = json!({"collectionName":"items","filter":filter,"outputFields":["count(*)"]});
        assert_eq!(
            ann.execute("entities/query", query.clone()).unwrap(),
            flat.execute("entities/query", query).unwrap()
        );
    }
    for filter in [
        "id >= 1000 and id < 1020",
        "id > 999999",
        "tag is null",
        "tag == 19 and rating > 14",
    ] {
        assert_eq!(search(&ann, filter), search(&flat, filter), "{filter}");
    }
    assert!(ann.index_status()["prunedSegments"].as_u64().unwrap() > 0);
    assert!(ann.index_status()["flatFallbacks"].as_u64().unwrap() > 0);
}
#[test]
fn read_views_remain_consistent_during_concurrent_updates_and_index_publication() {
    let mut e = engine("HNSW", 2048);
    let view = Arc::new(e.read_view().unwrap());
    let expected = view
        .execute(
            "entities/query",
            json!({"collectionName":"items","outputFields":["count(*)"]}),
        )
        .unwrap();
    let barrier = Arc::new(Barrier::new(5));
    let mut readers = vec![];
    for _ in 0..4 {
        let v = view.clone();
        let b = barrier.clone();
        let expected = expected.clone();
        readers.push(thread::spawn(move || {
            b.wait();
            for _ in 0..10 {
                assert_eq!(
                    v.execute(
                        "entities/query",
                        json!({"collectionName":"items","outputFields":["count(*)"]})
                    )
                    .unwrap(),
                    expected
                );
                assert_eq!(
                    v.execute("entities/get", json!({"collectionName":"items","id":0}))
                        .unwrap()
                        .as_array()
                        .unwrap()
                        .len(),
                    1
                );
            }
        }));
    }
    barrier.wait();
    apply(
        &mut e,
        "entities/delete",
        json!({"collectionName":"items","ids":[0]}),
    );
    e.build_indexes().unwrap();
    for reader in readers {
        reader.join().unwrap();
    }
    assert_eq!(
        e.execute("entities/get", json!({"collectionName":"items","id":0}))
            .unwrap(),
        json!([])
    );
    assert!(e.index_status()["pinnedBytes"].as_u64().unwrap() > 0);
    drop(view);
    assert_eq!(e.index_status()["pinnedBytes"], 0);
}
#[test]
fn cancelled_and_dropped_builds_retry_without_publishing_partial_indexes() {
    let e = engine("HNSW", 512);
    e.request_index_build();
    let jobs = e.index_jobs();
    assert!(!jobs.is_empty());
    e.cancel_index_builds();
    for job in jobs {
        job.run();
    }
    assert_eq!(e.index_status()["builtSegments"], 0);
    assert!(
        e.index_status()["tasks"]
            .as_array()
            .unwrap()
            .iter()
            .all(|t| t["state"] == "cancelled")
    );
    assert!(
        e.index_jobs().is_empty(),
        "cancel also pauses unscheduled segments"
    );
    e.request_index_build();
    let jobs = e.index_jobs();
    drop(jobs);
    assert!(
        e.index_status()["tasks"]
            .as_array()
            .unwrap()
            .iter()
            .any(|t| t["state"] == "failed")
    );
    e.build_indexes().unwrap();
    assert_eq!(e.index_status()["builtSegments"], 32);
}
#[test]
fn admission_and_eviction_are_bounded_and_release_reservations() {
    let budget = Budget::new(10);
    let reservation = budget.acquire(7).unwrap();
    assert!(budget.acquire(4).is_err());
    drop(reservation);
    assert_eq!(budget.used(), 0);
    let mut e = SegmentEngine::with_query_limits(20_000, 1024, 1024 * 1024, 16 * 1024 * 1024);
    apply(&mut e, "collections/create", create("IVF_FLAT"));
    apply(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":data(1024)}),
    );
    assert!(e.read_view().is_err());
    let _ = e.build_indexes();
    let status = e.index_status();
    assert!(status["cacheBytes"].as_u64().unwrap() <= 20_000);
    assert!(status["residentIndexBytes"].as_u64().unwrap() <= 20_000);
    assert_eq!(status["buildBytes"], 0);
    assert_eq!(
        search(&e, "id > 1000"),
        search(&engine("FLAT", 1024), "id > 1000")
    );
    let mut blocked = SegmentEngine::with_query_limits(1024, 1024, 1, 1);
    apply(&mut blocked, "collections/create", create("HNSW"));
    apply(
        &mut blocked,
        "entities/insert",
        json!({"collectionName":"items","data":data(10)}),
    );
    assert!(blocked.build_indexes().is_err());
    assert!(
        blocked
            .execute("entities/query", json!({"collectionName":"items"}))
            .is_err()
    );
}
#[test]
fn corrupted_index_metadata_graph_and_missing_objects_fail_closed() {
    let e = engine("HNSW", 256);
    e.build_indexes().unwrap();
    let sealed = e.seal().unwrap();
    assert!(!sealed.manifest.indexes.is_empty());
    let objects: BTreeMap<_, _> = sealed.objects.into_iter().collect();
    let key = sealed.manifest.indexes[0].key.clone();
    assert!(
        SegmentEngine::load(sealed.manifest.clone(), |k| if k == key {
            Err(vectordb_engine::Error::Integrity("missing index".into()))
        } else {
            Ok(objects[k].clone())
        })
        .is_err()
    );
    assert!(
        SegmentEngine::load(sealed.manifest.clone(), |k| {
            let mut b = objects[k].clone();
            if k == key {
                b[0] ^= 1;
            }
            Ok(b)
        })
        .is_err()
    );
    for field in ["format", "dtype", "dimension", "graph", "coverage"] {
        let mut value: Value = serde_json::from_slice(&objects[&key]).unwrap();
        match field {
            "format" => value["format"] = json!(99),
            "dtype" => value["dtype"] = json!("Float64"),
            "dimension" => value["dimensions"]["vector"] = json!(17),
            "graph" => value["ann"]["vector"]["Hnsw"]["entry"] = json!(999999),
            _ => {
                value["keys"].as_array_mut().unwrap().pop();
            }
        }
        let bad = serde_json::to_vec(&value).unwrap();
        let mut manifest = sealed.manifest.clone();
        manifest.indexes[0].sha256 = checksum(&bad);
        manifest.indexes[0].bytes = bad.len();
        assert!(
            SegmentEngine::load(manifest, |k| Ok(if k == key {
                bad.clone()
            } else {
                objects[k].clone()
            }))
            .is_err(),
            "{field}"
        );
    }
}
#[test]
fn format_two_raw_snapshots_remain_readable() {
    let e = engine("FLAT", 32);
    let s = e.seal().unwrap();
    let mut m = s.manifest;
    m.format = 2;
    let objects: BTreeMap<_, _> = s.objects.into_iter().collect();
    let recovered = SegmentEngine::load(m, |k| Ok(objects[k].clone())).unwrap();
    assert_eq!(search(&e, ""), search(&recovered, ""));
}
#[test]
fn sparse_binary_and_partitioned_queries_use_same_visibility_after_index_reload() {
    let mut e = SegmentEngine::new();
    apply(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","schema":{"fields":[{"fieldName":"id","dataType":"Int64","isPrimary":true},{"fieldName":"b","dataType":"BinaryVector","elementTypeParams":{"dim":8}},{"fieldName":"s","dataType":"SparseFloatVector"}]}}),
    );
    apply(
        &mut e,
        "partitions/create",
        json!({"collectionName":"items","partitionName":"p"}),
    );
    let data = (0..1024)
        .map(|i| json!({"id":i,"b":[i%256],"s":{"3":(i%7) as f64,"9":0.5}}))
        .collect::<Vec<_>>();
    apply(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","partitionName":"p","data":data}),
    );
    let requests = [
        json!({"collectionName":"items","annsField":"b","data":[[3]],"filter":"id > 500","partitionNames":["p"],"limit":10}),
        json!({"collectionName":"items","annsField":"s","data":[{"3":1.0}],"filter":"id < 200","limit":10}),
    ];
    let expected = requests
        .iter()
        .map(|r| e.execute("entities/search", r.clone()).unwrap())
        .collect::<Vec<_>>();
    e.build_indexes().unwrap();
    let restored = restore(&e);
    for (r, expected) in requests.into_iter().zip(expected) {
        assert_eq!(restored.execute("entities/search", r).unwrap(), expected);
    }
    assert_eq!(
        restored
            .execute(
                "entities/get",
                json!({"collectionName":"items","id":[3,3,5],"partitionNames":["_default"]})
            )
            .unwrap(),
        json!([])
    );
    assert_eq!(
        restored
            .execute(
                "entities/get",
                json!({"collectionName":"items","id":[3,3,5]})
            )
            .unwrap()
            .as_array()
            .unwrap()
            .len(),
        2
    );
}
#[test]
fn malformed_ivf_buckets_are_rejected_before_search() {
    let e = engine("IVF_FLAT", 256);
    e.build_indexes().unwrap();
    let s = e.seal().unwrap();
    let mut manifest = s.manifest;
    let mut objects: BTreeMap<_, _> = s.objects.into_iter().collect();
    let reference = &mut manifest.indexes[0];
    let mut index: Value = serde_json::from_slice(&objects[&reference.key]).unwrap();
    index["ann"]["vector"]["Ivf"]["buckets"][0] = json!([usize::MAX]);
    let bytes = serde_json::to_vec(&index).unwrap();
    reference.bytes = bytes.len();
    reference.sha256 = checksum(&bytes);
    objects.insert(reference.key.clone(), bytes);
    assert!(SegmentEngine::load(manifest, |k| Ok(objects[k].clone())).is_err());
}
