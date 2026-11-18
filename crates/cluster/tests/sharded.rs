use serde_json::{Value, json};
use std::collections::{BTreeMap, BTreeSet};
use vectordb_cluster::{
    sharded::{QueryRoutes, ShardPart, ShardQuery, ShardReplica, ShardReply},
    state::{Entry, FORMAT, Record, Replica, Scope, Token, idempotency_key, now_ms},
};
use vectordb_engine::{
    distributed_query::QueryPlan,
    segment::SegmentEngine,
    shard::{hash_v1, shard_for},
};
fn scope() -> Scope {
    Scope {
        cluster: "test".into(),
        collection: "items".into(),
        generation: "m4".into(),
    }
}
fn create() -> Value {
    json!({"collectionName":"items","shardCount":8,"schema":{"autoID":true,"fields":[{"fieldName":"id","dataType":"Int64","isPrimary":true},{"fieldName":"rank","dataType":"Int64"},{"fieldName":"vector","dataType":"FloatVector","elementTypeParams":{"dim":2}},{"fieldName":"other","dataType":"FloatVector","elementTypeParams":{"dim":2}}]},"indexParams":[{"fieldName":"vector","metricType":"L2"},{"fieldName":"other","metricType":"IP"}]})
}
fn writer() -> Replica {
    let mut r = Replica::empty(scope()).unwrap();
    r.apply(
        0,
        Record {
            format: FORMAT,
            scope: scope(),
            entry: Entry::Activate { epoch: 1 },
        },
    )
    .unwrap();
    r
}
fn append(writer: &mut Replica, op: &str, r: Value) -> Record {
    let key = idempotency_key(now_ms(), &format!("record{}", writer.offset));
    let record = writer.prepare(writer.epoch, &key, op, r).unwrap().unwrap();
    writer.apply(writer.offset + 3, record.clone()).unwrap();
    record
}
#[test]
fn hash_v1_and_shard_layout_are_stable_and_typed() {
    assert_eq!(hash_v1("1"), 0xaf63ac4c86019afc);
    assert_ne!(hash_v1("1"), hash_v1("\"1\""));
    for count in [1, 2, 4, 8, 16, 32] {
        vectordb_engine::shard::validate_count(count).unwrap();
        assert!(shard_for("\"x\"", count) < count);
    }
    assert!(vectordb_engine::shard::validate_count(3).is_err());
    let mut e = SegmentEngine::new();
    let b = e.prepare("collections/create", create()).unwrap();
    e.commit(b, 1).unwrap();
    assert!(
        e.prepare(
            "entities/upsert",
            json!({"collectionName":"items","shardCount":4,"data":[]})
        )
        .is_err()
    );
}
#[test]
fn selected_checkpoint_fetches_no_foreign_objects_and_preserves_indexes() {
    let mut e = SegmentEngine::new();
    let mut request = create();
    request["schema"]["autoID"] = json!(false);
    request["indexParams"][0]["indexType"] = json!("HNSW");
    let b = e.prepare("collections/create", request).unwrap();
    e.commit(b, 1).unwrap();
    let data = (0..1024)
        .map(|id| json!({"id":id,"rank":id%7,"vector":[id as f64/1024.,1.],"other":[1.,0.]}))
        .collect::<Vec<_>>();
    let b = e
        .prepare(
            "entities/insert",
            json!({"collectionName":"items","data":data}),
        )
        .unwrap();
    e.commit(b, 2).unwrap();
    e.build_indexes().unwrap();
    let sealed = e.seal().unwrap();
    assert_eq!(sealed.manifest.format, 4);
    let selected = BTreeSet::from([0, 3]);
    let objects: BTreeMap<_, _> = sealed.objects.into_iter().collect();
    let mut fetched = BTreeSet::new();
    let loaded = SegmentEngine::load_shards(sealed.manifest.clone(), selected.clone(), |key| {
        fetched.insert(key.to_string());
        Ok(objects[key].clone())
    })
    .unwrap();
    for segment in &sealed.manifest.segments {
        for object in [&segment.scalars, &segment.versions, &segment.vectors] {
            assert_eq!(
                fetched.contains(&object.key),
                selected.contains(&segment.shard.unwrap())
            );
        }
    }
    assert_eq!(
        loaded.row_count(),
        data.iter()
            .filter(|r| selected.contains(&shard_for(&r["id"].to_string(), 8)))
            .count()
    );
    assert!(loaded.row_count() < e.row_count());
    assert!(loaded.seal().is_err());
    for (shard, engine) in loaded.split_shards(&selected).unwrap() {
        assert_eq!(engine.index_status()["builtSegments"], 0);
        assert!(engine.index_status()["loadedSegments"].as_u64().unwrap() > 0);
        for id in 0..1024 {
            assert_eq!(
                engine.get_version(&json!(id)).is_some(),
                shard_for(&id.to_string(), 8) == shard
            );
        }
    }
    let mut corrupt = sealed.manifest;
    corrupt.segments[0].shard = Some(7);
    assert!(
        SegmentEngine::load_shards(corrupt, (0..8).collect(), |k| Ok(objects[k].clone())).is_err()
    );
}
#[test]
fn shared_wal_replay_matches_full_writer_for_updates_deletes_autoids_and_controls() {
    let mut w = writer();
    let mut shards = ShardReplica::empty(scope(), 8, &(0..8).collect()).unwrap();
    shards
        .apply(
            0,
            Record {
                format: FORMAT,
                scope: scope(),
                entry: Entry::Activate { epoch: 1 },
            },
        )
        .unwrap();
    let operations = vec![
        ("collections/create", create()),
        (
            "partitions/create",
            json!({"collectionName":"items","partitionName":"p"}),
        ),
        (
            "entities/insert",
            json!({"collectionName":"items","data":(0..100).map(|n|json!({"rank":n%7,"vector":[n as f64/10.,1.],"other":[n as f64,1.]})).collect::<Vec<_>>()}),
        ),
        (
            "entities/insert",
            json!({"collectionName":"items","partitionName":"p","data":[{"rank":8,"vector":[0.,0.],"other":[0.,1.]}]}),
        ),
        (
            "entities/upsert",
            json!({"collectionName":"items","data":[{"id":3,"rank":8,"vector":[0.,0.],"other":[0.,1.]},{"id":500,"rank":0,"vector":[0.,1.],"other":[1.,1.]}]}),
        ),
        (
            "entities/delete",
            json!({"collectionName":"items","ids":[1,5,101,900]}),
        ),
        (
            "partitions/drop",
            json!({"collectionName":"items","partitionNames":["p"]}),
        ),
        (
            "entities/insert",
            json!({"collectionName":"items","data":[{"rank":5,"vector":[1.,1.],"other":[1.,0.]}]}),
        ),
    ];
    for (op, r) in operations {
        let record = append(&mut w, op, r);
        shards.apply(w.offset, record).unwrap();
    }
    assert_eq!(shards.records, 9);
    let requests = vec![
        (
            "entities/count",
            json!({"collectionName":"items","filter":"rank >= 3"}),
        ),
        (
            "entities/query",
            json!({"collectionName":"items","outputFields":["rank"],"orderByFields":["rank:desc"],"offset":13,"limit":27}),
        ),
        (
            "entities/get",
            json!({"collectionName":"items","id":[3,500,501,900],"outputFields":["rank"]}),
        ),
        (
            "entities/search",
            json!({"collectionName":"items","annsField":"vector","data":[[0.,0.],[2.,1.]],"filter":"rank >= 2","offset":2,"limit":9,"outputFields":["rank"]}),
        ),
        (
            "entities/hybrid_search",
            json!({"collectionName":"items","search":[{"annsField":"vector","data":[[0.,0.]],"limit":15},{"annsField":"other","data":[[1.,0.]],"limit":15}],"rerank":{"strategy":"rrf","params":{"k":60}},"limit":12}),
        ),
        (
            "entities/hybrid_search",
            json!({"collectionName":"items","search":[{"annsField":"vector","data":[[0.,0.]],"limit":15},{"annsField":"other","data":[[1.,0.]],"limit":15}],"rerank":{"strategy":"weighted","params":{"weights":[0.7,0.3]}},"limit":12}),
        ),
    ];
    for (op, r) in requests {
        let plan = QueryPlan::new(w.spec().unwrap().clone(), op, r.clone()).unwrap();
        let parts = shards
            .engines
            .values()
            .map(|e| {
                plan.requests
                    .iter()
                    .map(|r| e.execute(&plan.operation, r.clone()).unwrap())
                    .collect()
            })
            .collect();
        assert_eq!(plan.merge(parts).unwrap(), w.query(op, r).unwrap(), "{op}");
    }
    let snapshot = w.checkpoint().unwrap();
    let objects: BTreeMap<_, _> = snapshot.sealed.objects.into_iter().collect();
    let restored = ShardReplica::restore(
        snapshot.checkpoint,
        &scope(),
        8,
        &BTreeSet::from([1, 2]),
        |k| Ok(objects[k].clone()),
    )
    .unwrap();
    assert!(restored.rows() < shards.rows());
    assert_eq!(restored.offset, w.offset);
    let token = Token {
        scope: scope(),
        offset: w.offset,
    };
    let views = shards.views(&token, &(0..8).collect()).unwrap();
    let record = append(
        &mut w,
        "entities/delete",
        json!({"collectionName":"items","ids":[3,500,501]}),
    );
    shards.apply(w.offset, record).unwrap();
    assert_eq!(
        views
            .iter()
            .map(|(_, v)| v
                .execute("entities/count", json!({"collectionName":"items"}))
                .unwrap()["count"]
                .as_u64()
                .unwrap())
            .sum::<u64>(),
        100
    );
    assert!(shards.views(&token, &BTreeSet::from([0])).is_err());
}
#[test]
fn incomplete_duplicate_or_mixed_snapshot_results_fail_closed() {
    let token = Token {
        scope: scope(),
        offset: 12,
    };
    let request = ShardQuery {
        token: token.clone(),
        route_epoch: 2,
        shard_count: 8,
        shards: BTreeSet::from([1, 2]),
        manifest: None,
        pin: None,
        operation: "entities/count".into(),
        requests: vec![json!({"collectionName":"items"})],
        deadline_ms: now_ms() + 1000,
        warm_version: None,
    };
    let mut reply = ShardReply {
        token,
        route_epoch: 2,
        parts: vec![
            ShardPart {
                shard: 1,
                values: vec![json!({"count":2})],
            },
            ShardPart {
                shard: 2,
                values: vec![json!({"count":3})],
            },
        ],
    };
    reply.validate(&request).unwrap();
    reply.token.offset += 1;
    assert!(reply.validate(&request).is_err());
    reply.token.offset -= 1;
    reply.parts[1].shard = 1;
    assert!(reply.validate(&request).is_err());
    reply.parts.pop();
    assert!(reply.validate(&request).is_err());
    assert!(
        QueryRoutes {
            epoch: 1,
            hash_version: 1,
            shards: vec![vec![]; 8]
        }
        .validate(8)
        .is_err()
    );
    assert!(
        QueryRoutes {
            epoch: 1,
            hash_version: 2,
            shards: vec![vec!["http://localhost:1".into()]; 8]
        }
        .validate(8)
        .is_err()
    );
}
#[test]
fn global_hybrid_ranks_and_ties_match_independent_flat_reference() {
    let mut w = writer();
    append(&mut w, "collections/create", create());
    let rows = (0..40)
        .map(|n| json!({"rank":n%3,"vector":[n as f64,0.],"other":[n as f64,0.]}))
        .collect::<Vec<_>>();
    append(
        &mut w,
        "entities/insert",
        json!({"collectionName":"items","data":rows}),
    );
    let r = json!({"collectionName":"items","search":[{"annsField":"vector","data":[[0.,0.]],"limit":40},{"annsField":"other","data":[[1.,0.]],"limit":40}],"rerank":{"strategy":"rrf","params":{"k":10}},"limit":10});
    let plan = QueryPlan::new(w.spec().unwrap().clone(), "entities/hybrid_search", r).unwrap();
    let snapshot = w.checkpoint().unwrap();
    let objects: BTreeMap<_, _> = snapshot.sealed.objects.into_iter().collect();
    let shards = ShardReplica::restore(snapshot.checkpoint, &scope(), 8, &(0..8).collect(), |k| {
        Ok(objects[k].clone())
    })
    .unwrap();
    let parts = shards
        .engines
        .values()
        .map(|e| {
            plan.requests
                .iter()
                .map(|r| e.execute("entities/search", r.clone()).unwrap())
                .collect()
        })
        .collect();
    let hits = plan.merge(parts).unwrap();
    let mut truth = (1..=40)
        .map(|id| (id, 1. / (10. + id as f64) + 1. / (10. + (41 - id) as f64)))
        .collect::<Vec<_>>();
    truth.sort_by(|a, b| {
        b.1.total_cmp(&a.1)
            .then_with(|| a.0.to_string().cmp(&b.0.to_string()))
    });
    assert_eq!(
        hits.as_array()
            .unwrap()
            .iter()
            .map(|h| h["id"].as_i64().unwrap())
            .collect::<Vec<_>>(),
        truth.iter().take(10).map(|(id, _)| *id).collect::<Vec<_>>()
    );
    for (hit, (_, score)) in hits.as_array().unwrap().iter().zip(truth) {
        assert!((hit["distance"].as_f64().unwrap() - score).abs() < 1e-12);
    }
}

#[test]
fn string_primary_keys_sparse_binary_and_hidden_sort_fields_match_reference() {
    let mut w = writer();
    let mut shards = ShardReplica::empty(scope(), 4, &(0..4).collect()).unwrap();
    shards
        .apply(
            0,
            Record {
                format: FORMAT,
                scope: scope(),
                entry: Entry::Activate { epoch: 1 },
            },
        )
        .unwrap();
    let operations = vec![
        (
            "collections/create",
            json!({"collectionName":"items","shardCount":4,"schema":{"enableDynamicField":true,"fields":[{"fieldName":"key","dataType":"VarChar","isPrimary":true,"elementTypeParams":{"max_length":32}},{"fieldName":"b","dataType":"BinaryVector","elementTypeParams":{"dim":8}},{"fieldName":"s","dataType":"SparseFloatVector"},{"fieldName":"rank","dataType":"Int64","nullable":true}]}}),
        ),
        (
            "entities/insert",
            json!({"collectionName":"items","data":(0..100).map(|n|json!({"key":format!("条目{n}"),"b":[n%256],"s":{"3":n%7,"9":0.5},"rank":if n%3==0 {Value::Null} else {json!(n%5)},"extra":n})).collect::<Vec<_>>()}),
        ),
        (
            "entities/delete",
            json!({"collectionName":"items","ids":["条目1","条目99","absent"]}),
        ),
    ];
    for (op, r) in operations {
        let record = append(&mut w, op, r);
        shards.apply(w.offset, record).unwrap();
    }
    for (op, r) in [
        (
            "entities/search",
            json!({"collectionName":"items","annsField":"b","data":[[3],[5]],"filter":"extra > 5","outputFields":["extra"],"limit":9,"offset":3}),
        ),
        (
            "entities/search",
            json!({"collectionName":"items","annsField":"s","data":[{"3":1.0}],"limit":13}),
        ),
        (
            "entities/query",
            json!({"collectionName":"items","orderByFields":["rank:desc","key:asc"],"outputFields":["b"],"offset":7,"limit":20}),
        ),
        (
            "entities/get",
            json!({"collectionName":"items","id":["条目0","条目1","条目50"],"outputFields":["extra"]}),
        ),
    ] {
        let plan = QueryPlan::new(w.spec().unwrap().clone(), op, r.clone()).unwrap();
        let parts = shards
            .engines
            .values()
            .map(|e| {
                plan.requests
                    .iter()
                    .map(|r| e.execute(&plan.operation, r.clone()).unwrap())
                    .collect()
            })
            .collect();
        assert_eq!(plan.merge(parts).unwrap(), w.query(op, r).unwrap());
    }
}

#[test]
fn malformed_committed_batch_cannot_partially_advance_local_shards() {
    let mut w = writer();
    let mut shards = ShardReplica::empty(scope(), 8, &(0..8).collect()).unwrap();
    shards
        .apply(
            0,
            Record {
                format: FORMAT,
                scope: scope(),
                entry: Entry::Activate { epoch: 1 },
            },
        )
        .unwrap();
    let create = append(&mut w, "collections/create", create());
    shards.apply(w.offset, create).unwrap();
    let offset = shards.offset;
    let good = append(
        &mut w,
        "entities/insert",
        json!({"collectionName":"items","data":(0..20).map(|n|json!({"rank":n,"vector":[0.,1.],"other":[1.,0.]})).collect::<Vec<_>>()}),
    );
    let mut bad = good.clone();
    if let Entry::Mutation { result, .. } = &mut bad.entry {
        result["insertCount"] = json!(19);
    }
    assert!(shards.apply(w.offset, bad).is_err());
    assert_eq!(shards.offset, offset);
    assert_eq!(shards.rows(), 0);
    shards.apply(w.offset, good).unwrap();
    assert_eq!(shards.rows(), 20);
}

#[test]
fn pre_sharding_manifest_without_layout_fields_loads_as_one_shard() {
    let mut w = writer();
    let mut r = create();
    r.as_object_mut().unwrap().remove("shardCount");
    append(&mut w, "collections/create", r);
    append(
        &mut w,
        "entities/insert",
        json!({"collectionName":"items","data":[{"rank":1,"vector":[0.,1.],"other":[1.,0.]}]}),
    );
    let snapshot = w.checkpoint().unwrap();
    let objects: BTreeMap<_, _> = snapshot.sealed.objects.into_iter().collect();
    let mut old = serde_json::to_value(snapshot.checkpoint).unwrap();
    old["state"]["spec"]
        .as_object_mut()
        .unwrap()
        .remove("shard_count");
    for segment in old["state"]["segments"].as_array_mut().unwrap() {
        segment.as_object_mut().unwrap().remove("shard");
    }
    let restored = ShardReplica::restore(
        serde_json::from_value(old).unwrap(),
        &scope(),
        1,
        &BTreeSet::from([0]),
        |k| Ok(objects[k].clone()),
    )
    .unwrap();
    assert_eq!(restored.rows(), 1);
    assert_eq!(
        restored.engines[&0].get_version(&json!(1)).unwrap().version,
        w.offset
    );
}
