use serde_json::{Value, json};
use vectordb_engine::{Engine, Error};
fn call(e: &mut Engine, op: &str, r: Value) -> Value {
    e.execute(op, r).unwrap_or_else(|err| panic!("{op}: {err}"))
}
fn base() -> Engine {
    let mut e = Engine::memory().unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","dimension":2}),
    );
    e
}
fn write(e: &mut Engine) {
    call(
        e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[1,0],"tag":"rust","price":12},{"id":2,"vector":[0,1],"tag":"go","price":5},{"id":3,"vector":[0.8,0.2],"tag":"rust","price":20}]}),
    );
}
fn count(e: &mut Engine) -> Value {
    call(
        e,
        "collections/get_stats",
        json!({"collectionName":"items"}),
    )["rowCount"]
        .clone()
}
#[test]
fn write_search_filter_delete() {
    let mut e = base();
    write(&mut e);
    let hits = call(
        &mut e,
        "entities/search",
        json!({"collectionName":"items","data":[[1,0]],"limit":2,"filter":"tag == 'rust' and price >= 10","outputFields":["tag"]}),
    );
    assert_eq!(hits[0]["id"], 1);
    assert_eq!(hits[1]["id"], 3);
    assert_eq!(hits[0]["tag"], "rust");
    assert!((hits[0]["distance"].as_f64().unwrap() - 1.0).abs() < 1e-9);
    assert_eq!(
        call(
            &mut e,
            "entities/delete",
            json!({"collectionName":"items","filter":"price < 10"})
        )["deleteCount"],
        1
    );
    assert_eq!(count(&mut e), 2);
}
#[test]
fn failed_batch_is_atomic() {
    let mut e = base();
    let err = e.execute(
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[1,0]},{"id":2,"vector":[1]}]}),
    );
    assert!(err.is_err());
    assert_eq!(count(&mut e), 0);
    write(&mut e);
    assert!(e.execute("entities/insert",json!({"collectionName":"items","data":[{"id":4,"vector":[1,0]},{"id":1,"vector":[1,0]}]})).is_err());
    assert_eq!(count(&mut e), 3);
}
#[test]
fn auto_id_rollback_restart_and_upsert() {
    let tmp = tempfile::tempdir().unwrap();
    let p = tmp.path().join("db");
    let mut e = Engine::open(&p).unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","dimension":2,"autoID":true}),
    );
    assert!(
        e.execute(
            "entities/insert",
            json!({"collectionName":"items","data":[{"vector":[1,0]},{"vector":[1]}]})
        )
        .is_err()
    );
    assert_eq!(
        call(
            &mut e,
            "entities/insert",
            json!({"collectionName":"items","data":[{"vector":[1,0]}]})
        )["insertIds"],
        json!([1])
    );
    drop(e);
    let mut e = Engine::open(&p).unwrap();
    assert_eq!(
        call(
            &mut e,
            "entities/insert",
            json!({"collectionName":"items","data":[{"vector":[0,1]}]})
        )["insertIds"],
        json!([2])
    );
    call(
        &mut e,
        "entities/upsert",
        json!({"collectionName":"items","data":[{"id":20,"vector":[1,1]}]}),
    );
    assert_eq!(
        call(
            &mut e,
            "entities/insert",
            json!({"collectionName":"items","data":[{"vector":[0,0]}]})
        )["insertIds"],
        json!([21])
    );
}
#[test]
fn partitions_aliases_and_isolation() {
    let mut e = base();
    write(&mut e);
    call(
        &mut e,
        "partitions/create",
        json!({"collectionName":"items","partitionName":"archive"}),
    );
    call(
        &mut e,
        "entities/upsert",
        json!({"collectionName":"items","partitionName":"archive","data":[{"id":1,"vector":[0,1]}]}),
    );
    let result = call(
        &mut e,
        "entities/query",
        json!({"collectionName":"items","partitionNames":["archive"],"outputFields":["*"]}),
    );
    assert_eq!(result.as_array().unwrap().len(), 1);
    assert_eq!(result[0]["vector"], json!([0, 1]));
    call(
        &mut e,
        "aliases/create",
        json!({"collectionName":"items","aliasName":"live"}),
    );
    call(
        &mut e,
        "collections/rename",
        json!({"collectionName":"items","newCollectionName":"renamed"}),
    );
    assert_eq!(
        call(
            &mut e,
            "collections/get_stats",
            json!({"collectionName":"live"})
        )["rowCount"],
        3
    );
    call(
        &mut e,
        "partitions/drop",
        json!({"collectionName":"live","partitionNames":["archive"]}),
    );
    assert_eq!(
        call(
            &mut e,
            "collections/get_stats",
            json!({"collectionName":"live"})
        )["rowCount"],
        2
    );
    call(&mut e, "databases/create", json!({"dbName":"tenant"}));
    call(
        &mut e,
        "collections/create",
        json!({"dbName":"tenant","collectionName":"renamed","dimension":3}),
    );
    assert_eq!(
        call(
            &mut e,
            "collections/get_stats",
            json!({"dbName":"tenant","collectionName":"renamed"})
        )["rowCount"],
        0
    );
    assert!(
        e.execute("databases/drop", json!({"dbName":"tenant"}))
            .is_err()
    );
}
#[test]
fn release_is_enforced() {
    let mut e = base();
    write(&mut e);
    call(
        &mut e,
        "collections/release",
        json!({"collectionName":"items"}),
    );
    assert!(
        e.execute(
            "entities/search",
            json!({"collectionName":"items","data":[[1,0]]})
        )
        .is_err()
    );
    call(
        &mut e,
        "collections/load",
        json!({"collectionName":"items"}),
    );
    call(
        &mut e,
        "partitions/release",
        json!({"collectionName":"items","partitionNames":["_default"]}),
    );
    assert!(
        e.execute("entities/query", json!({"collectionName":"items"}))
            .is_err()
    );
    call(
        &mut e,
        "partitions/load",
        json!({"collectionName":"items","partitionNames":["_default"]}),
    );
    assert_eq!(
        call(&mut e, "entities/query", json!({"collectionName":"items"}))
            .as_array()
            .unwrap()
            .len(),
        3
    );
}
#[test]
fn complex_filters_and_injection_rejection() {
    let mut e = base();
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[1,0],"name":"向量数据库","meta":{"region":"cn"},"tags":["rust","db"],"age":null},{"id":2,"vector":[0,1],"name":"other","meta":{"region":"us"},"tags":["db"],"age":30}]}),
    );
    for filter in [
        "name like '向量%'",
        "meta['region'] in ['cn']",
        "array_contains(tags, 'rust')",
        "age is null",
        "not (id != 1) and (id <= 1 or id > 2)",
        "age not in [30]",
    ] {
        let rows = call(
            &mut e,
            "entities/query",
            json!({"collectionName":"items","filter":filter}),
        );
        if filter == "age not in [30]" {
            assert_eq!(rows, json!([]));
        } else {
            assert_eq!(rows[0]["id"], 1);
            assert_eq!(rows.as_array().unwrap().len(), 1);
        }
    }
    assert!(
        e.execute(
            "entities/delete",
            json!({"collectionName":"items","filter":"id == 1; DROP TABLE entities"})
        )
        .is_err()
    );
    assert_eq!(count(&mut e), 2);
    assert!(
        e.execute(
            "entities/query",
            json!({"collectionName":"items","filter":"((((id == 1)"})
        )
        .is_err()
    );
}
#[test]
fn int64_comparison_is_exact() {
    let mut e = base();
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":9007199254740992i64,"vector":[1,0]},{"id":9007199254740993i64,"vector":[1,0]}]}),
    );
    let rows = call(
        &mut e,
        "entities/query",
        json!({"collectionName":"items","filter":"id == 9007199254740993"}),
    );
    assert_eq!(rows.as_array().unwrap().len(), 1);
    assert_eq!(rows[0]["id"], 9007199254740993i64);
}
#[test]
fn explicit_schema_defaults_and_types() {
    let mut e = Engine::memory().unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","schema":{"fields":[{"fieldName":"key","dataType":"VarChar","isPrimary":true,"elementTypeParams":{"max_length":8}},{"fieldName":"vector","dataType":"FloatVector","elementTypeParams":{"dim":2}},{"fieldName":"stock","dataType":"Int8","defaultValue":3},{"fieldName":"note","dataType":"VarChar","nullable":true}]}}),
    );
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"key":"a","vector":[1,0]}]}),
    );
    let row = call(
        &mut e,
        "entities/get",
        json!({"collectionName":"items","id":"a","outputFields":["*"]}),
    );
    assert_eq!(row[0]["stock"], 3);
    assert!(row[0]["note"].is_null());
    for row in [
        json!({"key":"b","vector":[1,0],"extra":1}),
        json!({"key":"b","vector":[1,0],"stock":128}),
        json!({"key":"too_long_key","vector":[1,0]}),
    ] {
        assert!(
            e.execute(
                "entities/insert",
                json!({"collectionName":"items","data":[row]})
            )
            .is_err()
        );
    }
}
#[test]
fn unsupported_options_never_succeed() {
    let mut e = base();
    for (op, r) in [
        (
            "entities/search",
            json!({"collectionName":"items","data":[[1,0]],"consistencyLevel":"Eventually"}),
        ),
        (
            "indexes/create",
            json!({"collectionName":"items","indexParams":[{"fieldName":"vector","indexType":"GPU_CAGRA"}]}),
        ),
        (
            "entities/search",
            json!({"collectionName":"items","data":[[1,0]],"searchParams":{"params":{"ef":12}}}),
        ),
        (
            "collections/create",
            json!({"collectionName":"bad","dimension":2,"numShards":8}),
        ),
    ] {
        assert!(e.execute(op, r).is_err());
    }
}
#[test]
fn metrics_and_multiple_queries() {
    for (metric, expected) in [
        ("COSINE", vec![1, 3, 2]),
        ("IP", vec![3, 1, 2]),
        ("L2", vec![1, 2, 3]),
    ] {
        let mut e = Engine::memory().unwrap();
        call(
            &mut e,
            "collections/create",
            json!({"collectionName":"items","dimension":2,"metricType":metric}),
        );
        call(
            &mut e,
            "entities/insert",
            json!({"collectionName":"items","data":[{"id":1,"vector":[1,0]},{"id":2,"vector":[0,1]},{"id":3,"vector":[3,1]}]}),
        );
        let r = call(
            &mut e,
            "entities/search",
            json!({"collectionName":"items","data":[[1,0],[0,1]],"limit":3}),
        );
        assert_eq!(
            r.as_array()
                .unwrap()
                .iter()
                .take(3)
                .map(|v| v["id"].as_i64().unwrap())
                .collect::<Vec<_>>(),
            expected
        );
        assert_eq!(r[3]["queryIndex"], 1);
    }
}
#[test]
fn sparse_binary_and_hybrid() {
    let mut e = Engine::memory().unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","schema":{"fields":[{"fieldName":"id","dataType":"Int64","isPrimary":true},{"fieldName":"dense","dataType":"FloatVector","elementTypeParams":{"dim":2}},{"fieldName":"sparse","dataType":"SparseFloatVector"},{"fieldName":"binary","dataType":"BinaryVector","elementTypeParams":{"dim":8}}]}}),
    );
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"dense":[1,0],"sparse":{"7":2},"binary":[15]},{"id":2,"dense":[0,1],"sparse":{"8":3},"binary":[255]}]}),
    );
    let a = call(
        &mut e,
        "entities/search",
        json!({"collectionName":"items","annsField":"sparse","data":[{"8":1}]}),
    );
    assert_eq!(a[0]["id"], 2);
    assert_eq!(a[0]["distance"], 3.0);
    let b = call(
        &mut e,
        "entities/search",
        json!({"collectionName":"items","annsField":"binary","data":[[15]]}),
    );
    assert_eq!(b[0]["id"], 1);
    assert_eq!(b[1]["distance"], 4.0);
    for rerank in [
        json!({"strategy":"rrf","params":{"k":60}}),
        json!({"strategy":"weighted","params":{"weights":[0.1,0.9]}}),
    ] {
        let h = call(
            &mut e,
            "entities/hybrid_search",
            json!({"collectionName":"items","search":[{"annsField":"dense","data":[[1,0]],"limit":2},{"annsField":"sparse","data":[{"8":1}],"limit":2}],"rerank":rerank,"limit":2}),
        );
        assert_eq!(h.as_array().unwrap().len(), 2);
        if rerank["strategy"] == "weighted" {
            assert_eq!(h[0]["id"], 2);
        }
    }
}
#[test]
fn ivf_full_probe_matches_flat_and_invalidates() {
    let mut e = base();
    let rows: Vec<_> = (0..128)
        .map(|i| json!({"id":i,"vector":[(i as f64).sin(),(i as f64).cos()]}))
        .collect();
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":rows}),
    );
    let request = json!({"collectionName":"items","data":[[1,0]],"limit":12});
    let exact = call(&mut e, "entities/search", request.clone());
    call(
        &mut e,
        "indexes/drop",
        json!({"collectionName":"items","indexName":"vector"}),
    );
    call(
        &mut e,
        "indexes/create",
        json!({"collectionName":"items","indexParams":[{"fieldName":"vector","indexType":"IVF_FLAT","params":{"nlist":8}}]}),
    );
    let mut req = request.clone();
    req["searchParams"] = json!({"params":{"nprobe":8}});
    assert_eq!(call(&mut e, "entities/search", req.clone()), exact);
    call(
        &mut e,
        "entities/upsert",
        json!({"collectionName":"items","data":[{"id":999,"vector":[1,0]}]}),
    );
    assert_eq!(call(&mut e, "entities/search", req.clone())[0]["id"], 999);
    call(
        &mut e,
        "entities/delete",
        json!({"collectionName":"items","filter":"id == 999"}),
    );
    assert_eq!(call(&mut e, "entities/search", req), exact);
}
#[test]
fn range_group_order_and_count() {
    let mut e = base();
    write(&mut e);
    let r = call(
        &mut e,
        "entities/search",
        json!({"collectionName":"items","data":[[1,0]],"searchParams":{"params":{"radius":0.5}},"groupingField":"tag"}),
    );
    assert_eq!(r.as_array().unwrap().len(), 1);
    assert_eq!(r[0]["id"], 1);
    let r = call(
        &mut e,
        "entities/query",
        json!({"collectionName":"items","orderByFields":["id:desc"],"offset":1,"limit":1}),
    );
    assert_eq!(r[0]["id"], 2);
    assert_eq!(
        call(
            &mut e,
            "entities/query",
            json!({"collectionName":"items","filter":"tag == 'rust'","outputFields":["count(*)"]})
        )[0]["count(*)"],
        2
    );
}
#[test]
fn snapshots_and_exclusive_lock() {
    let tmp = tempfile::tempdir().unwrap();
    let path = tmp.path().join("live.sqlite");
    let mut e = Engine::open(&path).unwrap();
    assert!(matches!(Engine::open(&path), Err(Error::Conflict(_))));
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","dimension":2}),
    );
    write(&mut e);
    let snapshot = tmp.path().join("backup.sqlite");
    e.backup(&snapshot).unwrap();
    assert!(e.backup(&snapshot).is_err());
    call(
        &mut e,
        "entities/delete",
        json!({"collectionName":"items","filter":"id > 0"}),
    );
    let mut restored = Engine::open(&snapshot).unwrap();
    assert_eq!(count(&mut restored), 3);
    assert_eq!(count(&mut e), 0);
}
#[test]
fn ddl_batch_rollback_and_cascade() {
    let mut e = base();
    write(&mut e);
    call(
        &mut e,
        "partitions/create",
        json!({"collectionName":"items","partitionName":"extra"}),
    );
    assert!(
        e.execute(
            "partitions/drop",
            json!({"collectionName":"items","partitionNames":["extra","_default"]})
        )
        .is_err()
    );
    assert_eq!(
        call(
            &mut e,
            "partitions/has",
            json!({"collectionName":"items","partitionName":"extra"})
        )["has"],
        true
    );
    call(
        &mut e,
        "aliases/create",
        json!({"collectionName":"items","aliasName":"alias"}),
    );
    call(
        &mut e,
        "collections/drop",
        json!({"collectionName":"items"}),
    );
    assert!(
        e.execute("aliases/describe", json!({"aliasName":"alias"}))
            .is_err()
    );
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","dimension":2}),
    );
    assert_eq!(count(&mut e), 0);
}

#[test]
fn hnsw_recall_and_cache_refresh() {
    let mut e = Engine::memory().unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","dimension":8,"metricType":"L2"}),
    );
    let mut seed = 42u64;
    let mut next = || {
        seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1);
        (seed >> 32) as f64 / u32::MAX as f64
    };
    let rows: Vec<_> = (0..400)
        .map(|i| json!({"id":i,"vector":(0..8).map(|_|next()).collect::<Vec<_>>()}))
        .collect();
    let queries: Vec<_> = (0..12)
        .map(|_| json!((0..8).map(|_| next()).collect::<Vec<_>>()))
        .collect();
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":rows}),
    );
    let req = json!({"collectionName":"items","data":queries,"limit":10});
    let exact = call(&mut e, "entities/search", req.clone());
    call(
        &mut e,
        "indexes/drop",
        json!({"collectionName":"items","indexName":"vector"}),
    );
    call(
        &mut e,
        "indexes/create",
        json!({"collectionName":"items","indexParams":[{"fieldName":"vector","indexType":"HNSW","metricType":"L2","params":{"M":12,"efConstruction":100}}]}),
    );
    let mut req = req;
    req["searchParams"] = json!({"params":{"ef":100}});
    let ann = call(&mut e, "entities/search", req);
    let matched = ann
        .as_array()
        .unwrap()
        .iter()
        .filter(|h| {
            exact
                .as_array()
                .unwrap()
                .iter()
                .any(|e| h["id"] == e["id"] && h["queryIndex"] == e["queryIndex"])
        })
        .count();
    assert!(matched as f64 / 120.0 >= 0.95, "recall {matched}/120");
    call(
        &mut e,
        "entities/upsert",
        json!({"collectionName":"items","data":[{"id":999,"vector":queries[0]}]}),
    );
    let r = call(
        &mut e,
        "entities/search",
        json!({"collectionName":"items","data":[queries[0]],"limit":1}),
    );
    assert_eq!(r[0]["id"], 999);
    assert_eq!(r[0]["distance"], 0.0);
}
#[test]
fn null_logic_empty_indexes_and_bad_parameters() {
    let mut e = base();
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"items","data":[{"id":1,"vector":[0,0],"value":null}]}),
    );
    for filter in [
        "not (value == 10)",
        "value not in [10]",
        "id not in [2,null]",
    ] {
        assert_eq!(
            call(
                &mut e,
                "entities/query",
                json!({"collectionName":"items","filter":filter})
            ),
            json!([])
        );
    }
    assert_eq!(
        call(
            &mut e,
            "entities/query",
            json!({"collectionName":"items","filter":"value is null or value > 1"})
        )[0]["id"],
        1
    );
    call(
        &mut e,
        "collections/truncate",
        json!({"collectionName":"items"}),
    );
    for kind in ["IVF_FLAT", "HNSW"] {
        call(
            &mut e,
            "indexes/drop",
            json!({"collectionName":"items","indexName":"vector"}),
        );
        call(
            &mut e,
            "indexes/create",
            json!({"collectionName":"items","indexParams":[{"fieldName":"vector","indexType":kind}]}),
        );
        assert_eq!(
            call(
                &mut e,
                "entities/search",
                json!({"collectionName":"items","data":[[1,0]]})
            ),
            json!([])
        );
    }
    for dimension in [json!(0), json!(-1), json!(3.5), json!(999999)] {
        assert!(
            e.execute(
                "collections/create",
                json!({"collectionName":"bad","dimension":dimension})
            )
            .is_err()
        );
    }
    assert_eq!(
        call(&mut e, "collections/has", json!({"collectionName":"bad"}))["has"],
        false
    );
}
#[test]
fn corrupt_database_is_an_error() {
    let tmp = tempfile::tempdir().unwrap();
    let path = tmp.path().join("corrupt");
    std::fs::write(&path, b"not a SQLite database").unwrap();
    assert!(matches!(Engine::open(path), Err(Error::Storage(_))));
}

#[test]
fn malformed_optional_values_are_rejected() {
    let mut e = base();
    for (op, r) in [
        (
            "collections/create",
            json!({"collectionName":"bad","dimension":2,"autoID":"false"}),
        ),
        (
            "collections/create",
            json!({"collectionName":"bad","dimension":2,"metricType":7}),
        ),
        (
            "entities/insert",
            json!({"collectionName":"items","partitionName":42,"data":[{"id":1,"vector":[1,0]}]}),
        ),
        (
            "entities/search",
            json!({"collectionName":"items","annsField":false,"data":[[1,0]]}),
        ),
        (
            "entities/query",
            json!({"collectionName":"items","orderByFields":["id:asc:desc"]}),
        ),
    ] {
        assert!(e.execute(op, r).is_err(), "{op} accepted malformed option");
    }
    assert_eq!(count(&mut e), 0);
}
#[test]
fn storage_failure_rolls_back_the_whole_batch() {
    let tmp = tempfile::tempdir().unwrap();
    let path = tmp.path().join("db.sqlite");
    let mut e = Engine::open(&path).unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"items","dimension":2}),
    );
    // Fault injection only: a second SQLite handle adds a trigger that fails the second row.
    let fault = rusqlite::Connection::open(&path).unwrap();
    fault.execute_batch("CREATE TRIGGER injected_failure BEFORE INSERT ON entities WHEN NEW.pk='2' BEGIN SELECT RAISE(ABORT,'injected storage error'); END;").unwrap();
    let error=e.execute("entities/insert",json!({"collectionName":"items","data":[{"id":1,"vector":[1,0]},{"id":2,"vector":[0,1]}]})).unwrap_err();
    assert!(matches!(error, Error::Storage(_)));
    assert_eq!(error.code(), 65535);
    assert_eq!(count(&mut e), 0);
    fault
        .execute_batch("DROP TRIGGER injected_failure")
        .unwrap();
    drop(fault);
    write(&mut e);
    assert_eq!(count(&mut e), 3);
}

#[test]
fn jsonl_roundtrip_and_batch_failure() {
    let mut source = base();
    write(&mut source);
    let mut bytes = Vec::new();
    assert_eq!(
        source.export_jsonl("default", "items", &mut bytes).unwrap(),
        3
    );
    let mut target = base();
    assert_eq!(
        target
            .import_jsonl(
                "default",
                "items",
                std::io::Cursor::new(bytes.clone()),
                false,
                2
            )
            .unwrap(),
        3
    );
    let query = json!({"collectionName":"items","data":[[1,0]],"outputFields":["*"]});
    assert_eq!(
        call(&mut source, "entities/search", query.clone()),
        call(&mut target, "entities/search", query)
    );
    assert!(
        target
            .import_jsonl("default", "items", std::io::Cursor::new(bytes), false, 2)
            .is_err()
    );
    assert_eq!(count(&mut target), 3);
    let input = b"{\"id\":4,\"vector\":[1,0]}\n{\"id\":5,\"vector\":[0,1]}\n{broken\n";
    assert!(
        target
            .import_jsonl("default", "items", std::io::Cursor::new(input), false, 2)
            .is_err()
    );
    assert_eq!(count(&mut target), 5);
}

#[test]
fn quick_sparse_and_varchar_primary() {
    let mut e = Engine::memory().unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"sparse","idType":"VarChar","vectorFieldType":"SparseFloatVector"}),
    );
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"sparse","data":[{"id":"你好","vector":{"42":2}}]}),
    );
    assert_eq!(
        call(
            &mut e,
            "entities/search",
            json!({"collectionName":"sparse","data":[{"42":3}]})
        )[0]["distance"],
        6.0
    );
}

#[test]
fn explicit_binary_metric_is_respected() {
    let mut e = Engine::memory().unwrap();
    call(
        &mut e,
        "collections/create",
        json!({"collectionName":"bits","vectorFieldType":"BinaryVector","dimension":8,"metricType":"JACCARD"}),
    );
    call(
        &mut e,
        "entities/insert",
        json!({"collectionName":"bits","data":[{"id":1,"vector":[3]}]}),
    );
    assert_eq!(
        call(
            &mut e,
            "entities/search",
            json!({"collectionName":"bits","data":[[1]]})
        )[0]["distance"],
        0.5
    );
    assert!(
        e.execute(
            "collections/create",
            json!({"collectionName":"bad","vectorFieldType":"SparseFloatVector","metricType":"L2"})
        )
        .is_err()
    );
}
