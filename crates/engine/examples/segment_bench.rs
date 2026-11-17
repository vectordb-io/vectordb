//! Deterministic M0/M1 baseline. Usage: segment_bench [rows] [dim] [seed] [updates].
use serde_json::json;
use std::{collections::BTreeMap, time::Instant};
use vectordb_engine::segment::SegmentEngine;
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = std::env::args()
        .skip(1)
        .map(|s| s.parse::<usize>())
        .collect::<Result<Vec<_>, _>>()?;
    let rows = args.first().copied().unwrap_or(10_000);
    let dim = args.get(1).copied().unwrap_or(128);
    let seed = args.get(2).copied().unwrap_or(24301);
    let updates = args.get(3).copied().unwrap_or(1000);
    let mut random = seed as u64;
    let mut vector = || {
        (0..dim)
            .map(|_| {
                random = random.wrapping_mul(6364136223846793005).wrapping_add(1);
                ((random >> 32) as u32 as f64 / u32::MAX as f64) as f32
            })
            .collect::<Vec<_>>()
    };
    let mut engine = SegmentEngine::new();
    let b = engine.prepare(
        "collections/create",
        json!({"collectionName":"bench","dimension":dim,"metricType":"L2"}),
    )?;
    engine.commit(b, 1)?;
    let start = Instant::now();
    for start in (0..rows).step_by(1000) {
        let data = (start..(start + 1000).min(rows))
            .map(|id| json!({"id":id,"vector":vector(),"group":id%10}))
            .collect::<Vec<_>>();
        let batch = engine.prepare(
            "entities/insert",
            json!({"collectionName":"bench","data":data}),
        )?;
        engine.commit(batch, engine.revision() + 1)?;
    }
    let build_ms = start.elapsed().as_secs_f64() * 1000.;
    let mut latencies = vec![];
    for _ in 0..updates {
        let data = json!({"collectionName":"bench","data":[{"id":0,"vector":vector(),"group":0}]});
        let start = Instant::now();
        let b = engine.prepare("entities/upsert", data)?;
        engine.commit(b, engine.revision() + 1)?;
        latencies.push(start.elapsed().as_secs_f64() * 1000.);
    }
    latencies.sort_by(f64::total_cmp);
    let p99 = latencies
        .get(latencies.len().saturating_sub(1) * 99 / 100)
        .copied()
        .unwrap_or(0.);
    let query = vector();
    let mut query_ms = vec![];
    for _ in 0..20 {
        let start = Instant::now();
        engine.execute(
            "entities/search",
            json!({"collectionName":"bench","data":[query],"limit":10}),
        )?;
        query_ms.push(start.elapsed().as_secs_f64() * 1000.);
    }
    query_ms.sort_by(f64::total_cmp);
    let start = Instant::now();
    let sealed = engine.seal()?;
    let seal_ms = start.elapsed().as_secs_f64() * 1000.;
    let object_bytes: usize = sealed.objects.iter().map(|(_, v)| v.len()).sum();
    let objects: BTreeMap<_, _> = sealed.objects.into_iter().collect();
    let start = Instant::now();
    let recovered = SegmentEngine::load(sealed.manifest, |key| Ok(objects[key].clone()))?;
    let restore_ms = start.elapsed().as_secs_f64() * 1000.;
    assert_eq!(recovered.row_count(), rows);
    println!(
        "{}",
        json!({"format":1,"workload":"segment-m1","rows":rows,"dimension":dim,"seed":seed,"updates":updates,"dtype":"Float32","top_k":10,"query_samples":20,"profile":if cfg!(debug_assertions){"debug"}else{"release"},"arch":std::env::consts::ARCH,"os":std::env::consts::OS,"build_ms":build_ms,"update_p99_ms":p99,"search_p99_ms":query_ms[19],"seal_ms":seal_ms,"restore_ms":restore_ms,"state_charge_bytes":engine.state_bytes(),"object_bytes":object_bytes})
    );
    Ok(())
}
