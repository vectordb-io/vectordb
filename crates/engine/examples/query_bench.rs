//! Fixed-seed ANN/FLAT comparison with persisted reload and strict-filter recall.
use serde_json::{Value, json};
use std::{
    collections::{BTreeMap, BTreeSet},
    time::Instant,
};
use vectordb_engine::segment::SegmentEngine;
fn apply(e: &mut SegmentEngine, op: &str, r: Value) {
    let b = e.prepare(op, r).unwrap();
    e.commit(b, e.revision() + 1).unwrap();
}
fn make(kind: &str, data: &[Value]) -> SegmentEngine {
    let mut e = SegmentEngine::new();
    let params = match kind {
        "HNSW" => json!({"M":16,"efConstruction":128}),
        "IVF_FLAT" => json!({"nlist":32}),
        _ => json!({}),
    };
    apply(
        &mut e,
        "collections/create",
        json!({"collectionName":"bench","schema":{"fields":[{"fieldName":"id","dataType":"Int64","isPrimary":true},{"fieldName":"tag","dataType":"Int64"},{"fieldName":"vector","dataType":"FloatVector","elementTypeParams":{"dim":32}}]},"indexParams":[{"fieldName":"vector","indexType":kind,"metricType":"L2","params":params}]}),
    );
    for chunk in data.chunks(1000) {
        apply(
            &mut e,
            "entities/insert",
            json!({"collectionName":"bench","data":chunk}),
        );
    }
    e
}
fn search(e: &SegmentEngine, q: &Value, filter: &str, params: Value) -> Value {
    e.execute("entities/search",json!({"collectionName":"bench","data":[q],"limit":10,"filter":filter,"searchParams":{"params":params}})).unwrap()
}
fn ids(v: &Value) -> BTreeSet<i64> {
    v.as_array()
        .unwrap()
        .iter()
        .map(|r| r["id"].as_i64().unwrap())
        .collect()
}
fn main() {
    let rows = std::env::args()
        .nth(1)
        .map(|s| s.parse().unwrap())
        .unwrap_or(10000usize);
    let mut seed = 24301u64;
    let mut vector = || {
        json!(
            (0..32)
                .map(|_| {
                    seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1);
                    ((seed >> 32) as u32 as f64 / u32::MAX as f64) as f32
                })
                .collect::<Vec<_>>()
        )
    };
    let data = (0..rows)
        .map(|id| json!({"id":id,"tag":id%100,"vector":vector()}))
        .collect::<Vec<_>>();
    let queries = (0..50).map(|_| vector()).collect::<Vec<_>>();
    let flat = make("FLAT", &data);
    let truth = queries
        .iter()
        .map(|q| ids(&search(&flat, q, "", json!({}))))
        .collect::<Vec<_>>();
    let strict = queries
        .iter()
        .map(|q| ids(&search(&flat, q, "tag == 7", json!({}))))
        .collect::<Vec<_>>();
    let mut reports = vec![];
    for kind in ["FLAT", "HNSW", "IVF_FLAT"] {
        let mut e = make(kind, &data);
        let start = Instant::now();
        e.build_indexes().unwrap();
        let build = start.elapsed().as_secs_f64() * 1000.;
        let params = match kind {
            "HNSW" => json!({"ef":64}),
            "IVF_FLAT" => json!({"nprobe":16}),
            _ => json!({}),
        };
        let mut latencies = vec![];
        let mut correct = 0;
        let mut strict_correct = 0;
        let mut strict_ms = vec![];
        for (n, q) in queries.iter().enumerate() {
            let start = Instant::now();
            let hits = search(&e, q, "", params.clone());
            latencies.push(start.elapsed().as_secs_f64() * 1000.);
            correct += ids(&hits).intersection(&truth[n]).count();
            let start = Instant::now();
            let hits = search(&e, q, "tag == 7", params.clone());
            strict_ms.push(start.elapsed().as_secs_f64() * 1000.);
            strict_correct += ids(&hits).intersection(&strict[n]).count();
        }
        latencies.sort_by(f64::total_cmp);
        strict_ms.sort_by(f64::total_cmp);
        let snapshot = e.seal().unwrap();
        let index_bytes: usize = snapshot.manifest.indexes.iter().map(|i| i.bytes).sum();
        let objects: BTreeMap<_, _> = snapshot.objects.into_iter().collect();
        let start = Instant::now();
        let restored = SegmentEngine::load(snapshot.manifest, |k| Ok(objects[k].clone())).unwrap();
        let load_ms = start.elapsed().as_secs_f64() * 1000.;
        assert_eq!(restored.index_status()["builtSegments"], 0);
        assert_eq!(
            search(&restored, &queries[0], "", params.clone()),
            search(&e, &queries[0], "", params.clone())
        );
        let before = e.index_status()["builtSegments"].clone();
        let start = Instant::now();
        apply(
            &mut e,
            "entities/upsert",
            json!({"collectionName":"bench","data":[{"id":0,"tag":0,"vector":queries[0]}]}),
        );
        let update_ms = start.elapsed().as_secs_f64() * 1000.;
        assert!(e.index_jobs().is_empty());
        assert_eq!(e.index_status()["builtSegments"], before);
        let recall = correct as f64 / (queries.len() * 10) as f64;
        let strict_recall = strict_correct as f64 / (queries.len() * 10) as f64;
        assert!(recall >= 0.95, "{kind} recall {recall}");
        assert_eq!(strict_recall, 1.0);
        reports.push(json!({"index":kind,"search_params":params,"build_ms":build,"query_p50_ms":latencies[25],"query_p99_ms":latencies[49],"strict_filter_p99_ms":strict_ms[49],"recall_at_10":recall,"strict_recall_at_10":strict_recall,"index_object_bytes":index_bytes,"reload_ms":load_ms,"single_update_ms":update_ms,"status":e.index_status()}));
    }
    println!(
        "{}",
        json!({"format":1,"seed":24301,"rows":rows,"dimension":32,"dtype":"Float32","queries":50,"top_k":10,"filter_selectivity":0.01,"profile":if cfg!(debug_assertions){"debug"}else{"release"},"results":reports})
    );
}
