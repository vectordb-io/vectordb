//! Deterministic IVF_FLAT. Centroids are rebuilt lazily after writes; FLAT is exact.
use crate::{
    Result,
    error::invalid,
    schema::{Field, number},
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Index {
    pub field_name: String,
    #[serde(default)]
    pub index_name: String,
    #[serde(default = "flat")]
    pub index_type: String,
    #[serde(default = "cosine")]
    pub metric_type: String,
    #[serde(default = "object")]
    pub params: Value,
}
fn flat() -> String {
    "FLAT".into()
}
fn cosine() -> String {
    "COSINE".into()
}
fn object() -> Value {
    json!({})
}
impl Index {
    pub fn validate(&mut self, f: &Field) -> Result<()> {
        if self.index_name.is_empty() {
            self.index_name = self.field_name.clone();
        }
        crate::schema::name(&self.index_name)?;
        if !["FLAT", "IVF_FLAT", "HNSW"].contains(&self.index_type.as_str()) {
            return Err(crate::Error::Unsupported(format!(
                "index {} (supported: FLAT, IVF_FLAT, HNSW)",
                self.index_type
            )));
        }
        if self.index_type != "FLAT" && f.data_type != "FloatVector" {
            return Err(invalid("ANN indexes require FloatVector"));
        }
        let metrics = match f.data_type.as_str() {
            "FloatVector" => vec!["COSINE", "L2", "IP"],
            "SparseFloatVector" => vec!["IP"],
            "BinaryVector" => vec!["HAMMING", "JACCARD"],
            _ => return Err(invalid("index requires vector field")),
        };
        if !metrics.contains(&self.metric_type.as_str()) {
            return Err(invalid("metric is incompatible with vector type"));
        }
        if !self.params.is_object() {
            return Err(invalid("index params must be an object"));
        }
        for k in self.params.as_object().unwrap().keys() {
            if !match self.index_type.as_str() {
                "IVF_FLAT" => k == "nlist",
                "HNSW" => k == "M" || k == "efConstruction",
                _ => false,
            } {
                return Err(invalid(format!("unsupported index parameter {k}")));
            }
        }
        if number(&self.params["nlist"], 64, 4096, "nlist")? == 0 {
            return Err(invalid("nlist must be positive"));
        }
        let m = number(&self.params["M"], 16, 128, "M")?;
        let ef = number(&self.params["efConstruction"], 128, 4096, "efConstruction")?;
        if m < 2 || ef < m {
            return Err(invalid("HNSW requires M >= 2 and efConstruction >= M"));
        }
        Ok(())
    }
}
pub fn score(a: &Value, b: &Value, metric: &str) -> f64 {
    if let (Some(a), Some(b)) = (a.as_object(), b.as_object()) {
        return a
            .iter()
            .map(|(k, v)| v.as_f64().unwrap() * b.get(k).and_then(Value::as_f64).unwrap_or(0.0))
            .sum();
    }
    let a = a.as_array().expect("validated vector");
    let b = b.as_array().expect("validated vector");
    if metric == "HAMMING" || metric == "JACCARD" {
        let (mut xor, mut and, mut or) = (0, 0, 0);
        for (x, y) in a.iter().zip(b) {
            let (x, y) = (x.as_u64().unwrap(), y.as_u64().unwrap());
            xor += (x ^ y).count_ones();
            and += (x & y).count_ones();
            or += (x | y).count_ones();
        }
        return if metric == "HAMMING" {
            xor as f64
        } else if or == 0 {
            0.0
        } else {
            1.0 - and as f64 / or as f64
        };
    }
    let (mut dot, mut aa, mut bb, mut l2) = (0.0, 0.0, 0.0, 0.0);
    for (x, y) in a.iter().zip(b) {
        let (x, y) = (x.as_f64().unwrap(), y.as_f64().unwrap());
        dot += x * y;
        aa += x * x;
        bb += y * y;
        l2 += (x - y) * (x - y);
    }
    match metric {
        "L2" => l2,
        "IP" => dot,
        _ => {
            if aa == 0.0 || bb == 0.0 {
                0.0
            } else {
                (dot / (aa.sqrt() * bb.sqrt())).clamp(-1.0, 1.0)
            }
        }
    }
}
pub fn ascending(metric: &str) -> bool {
    matches!(metric, "L2" | "HAMMING" | "JACCARD")
}
pub struct Ivf {
    centroids: Vec<Value>,
    buckets: Vec<Vec<usize>>,
    metric: String,
}
impl Ivf {
    pub fn build(rows: &[Value], index: &Index) -> Self {
        let n = number(&index.params["nlist"], 64, 4096, "nlist")
            .unwrap()
            .min(rows.len());
        let metric = index.metric_type.clone();
        let field = &index.field_name;
        if n == 0 {
            return Self {
                centroids: vec![],
                buckets: vec![],
                metric,
            };
        }
        let mut centroids: Vec<Value> = (0..n)
            .map(|i| rows[i * rows.len() / n][field].clone())
            .collect();
        let dim = centroids[0].as_array().unwrap().len();
        for _ in 0..8 {
            let mut sums = vec![vec![0.0; dim]; n];
            let mut counts = vec![0; n];
            for row in rows {
                let v = &row[field];
                let j = nearest(v, &centroids, &metric);
                counts[j] += 1;
                let norm = if metric == "COSINE" {
                    v.as_array()
                        .unwrap()
                        .iter()
                        .map(|x| x.as_f64().unwrap().powi(2))
                        .sum::<f64>()
                        .sqrt()
                        .max(f64::MIN_POSITIVE)
                } else {
                    1.0
                };
                for (k, x) in v.as_array().unwrap().iter().enumerate() {
                    sums[j][k] += x.as_f64().unwrap() / norm;
                }
            }
            for j in 0..n {
                if counts[j] > 0 {
                    centroids[j] = json!(
                        sums[j]
                            .iter()
                            .map(|x| x / counts[j] as f64)
                            .collect::<Vec<_>>()
                    );
                }
            }
        }
        let mut buckets = vec![vec![]; n];
        for (i, row) in rows.iter().enumerate() {
            buckets[nearest(&row[field], &centroids, &metric)].push(i);
        }
        Self {
            centroids,
            buckets,
            metric,
        }
    }
    pub fn candidates(&self, q: &Value, nprobe: usize) -> Vec<usize> {
        let mut order: Vec<_> = self
            .centroids
            .iter()
            .enumerate()
            .map(|(i, v)| (i, score(q, v, &self.metric)))
            .collect();
        order.sort_by(|a, b| {
            if ascending(&self.metric) {
                a.1.total_cmp(&b.1)
            } else {
                b.1.total_cmp(&a.1)
            }
        });
        order
            .into_iter()
            .take(nprobe)
            .flat_map(|(i, _)| self.buckets[i].iter().copied())
            .collect()
    }
}
fn nearest(v: &Value, cs: &[Value], metric: &str) -> usize {
    cs.iter()
        .enumerate()
        .min_by(|(_, a), (_, b)| {
            let (x, y) = (score(v, a, metric), score(v, b, metric));
            if ascending(metric) {
                x.total_cmp(&y)
            } else {
                y.total_cmp(&x)
            }
        })
        .unwrap()
        .0
}

/// HNSW with deterministic geometric levels and nearest-neighbor pruning.
/// Based on Malkov & Yashunin (2018), algorithms 1, 2, 4 and 5.
pub struct Hnsw {
    vectors: Vec<Value>,
    links: Vec<Vec<Vec<usize>>>,
    entry: usize,
    top: usize,
    metric: String,
}
#[derive(Clone, Copy, Debug)]
struct Neighbor {
    distance: f64,
    id: usize,
}
impl PartialEq for Neighbor {
    fn eq(&self, b: &Self) -> bool {
        self.cmp(b).is_eq()
    }
}
impl Eq for Neighbor {}
impl PartialOrd for Neighbor {
    fn partial_cmp(&self, b: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(b))
    }
}
impl Ord for Neighbor {
    fn cmp(&self, b: &Self) -> std::cmp::Ordering {
        self.distance
            .total_cmp(&b.distance)
            .then_with(|| self.id.cmp(&b.id))
    }
}
impl Hnsw {
    pub fn build(rows: &[Value], index: &Index) -> Self {
        let m = number(&index.params["M"], 16, 128, "M").unwrap();
        let ef = number(&index.params["efConstruction"], 128, 4096, "efConstruction").unwrap();
        let mut graph = Self {
            vectors: vec![],
            links: vec![],
            entry: 0,
            top: 0,
            metric: index.metric_type.clone(),
        };
        for (i, row) in rows.iter().enumerate() {
            let q = row[&index.field_name].clone();
            // SplitMix64 gives stable pseudo-random levels without a runtime RNG.
            let mut seed = (i as u64).wrapping_add(0x9e3779b97f4a7c15);
            seed = (seed ^ (seed >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
            seed = (seed ^ (seed >> 27)).wrapping_mul(0x94d049bb133111eb);
            seed ^= seed >> 31;
            let u = ((seed >> 11) as f64 + 1.0) / ((1u64 << 53) as f64 + 1.0);
            let level = ((-u.ln() / (m as f64).ln()).floor() as usize).min(16);
            graph.vectors.push(q.clone());
            graph.links.push(vec![vec![]; level + 1]);
            if i == 0 {
                graph.top = level;
                continue;
            }
            let mut entry = graph.entry;
            for l in ((level + 1)..=graph.top).rev() {
                entry = graph.layer(&q, entry, l, 1)[0];
            }
            for l in (0..=level.min(graph.top)).rev() {
                let candidates = graph.layer(&q, entry, l, ef);
                entry = candidates[0];
                let selected: Vec<_> = candidates.into_iter().take(m).collect();
                graph.links[i][l] = selected.clone();
                for peer in selected {
                    graph.links[peer][l].push(i);
                    let cap = if l == 0 { 2 * m } else { m };
                    if graph.links[peer][l].len() > cap {
                        let mut neighbors = graph.links[peer][l].clone();
                        neighbors.sort_by(|a, b| {
                            graph
                                .distance(&graph.vectors[peer], *a)
                                .total_cmp(&graph.distance(&graph.vectors[peer], *b))
                                .then_with(|| a.cmp(b))
                        });
                        neighbors.truncate(cap);
                        graph.links[peer][l] = neighbors;
                    }
                }
            }
            if level > graph.top {
                graph.entry = i;
                graph.top = level;
            }
        }
        graph
    }
    fn distance(&self, q: &Value, i: usize) -> f64 {
        let d = score(q, &self.vectors[i], &self.metric);
        if ascending(&self.metric) { d } else { -d }
    }
    fn layer(&self, q: &Value, entry: usize, level: usize, ef: usize) -> Vec<usize> {
        use std::{cmp::Reverse, collections::BinaryHeap};
        let mut visited = vec![false; self.vectors.len()];
        visited[entry] = true;
        let first = Neighbor {
            distance: self.distance(q, entry),
            id: entry,
        };
        let mut frontier = BinaryHeap::from([Reverse(first)]);
        let mut best = BinaryHeap::from([first]);
        while let Some(Reverse(current)) = frontier.pop() {
            if best.len() >= ef && current.distance > best.peek().unwrap().distance {
                break;
            }
            for &peer in &self.links[current.id][level] {
                if visited[peer] {
                    continue;
                }
                visited[peer] = true;
                let candidate = Neighbor {
                    distance: self.distance(q, peer),
                    id: peer,
                };
                if best.len() < ef || candidate < *best.peek().unwrap() {
                    frontier.push(Reverse(candidate));
                    best.push(candidate);
                    if best.len() > ef {
                        best.pop();
                    }
                }
            }
        }
        best.into_sorted_vec().into_iter().map(|n| n.id).collect()
    }
    pub fn candidates(&self, q: &Value, ef: usize) -> Vec<usize> {
        if self.vectors.is_empty() {
            return vec![];
        }
        let mut entry = self.entry;
        for level in (1..=self.top).rev() {
            entry = self.layer(q, entry, level, 1)[0];
        }
        self.layer(q, entry, 0, ef)
    }
}
pub enum BuiltIndex {
    Ivf(Ivf),
    Hnsw(Hnsw),
}
impl BuiltIndex {
    pub fn build(rows: &[Value], index: &Index) -> Self {
        if index.index_type == "HNSW" {
            Self::Hnsw(Hnsw::build(rows, index))
        } else {
            Self::Ivf(Ivf::build(rows, index))
        }
    }
    pub fn candidates(&self, q: &Value, nprobe: usize, ef: usize) -> Vec<usize> {
        match self {
            Self::Ivf(i) => i.candidates(q, nprobe),
            Self::Hnsw(i) => i.candidates(q, ef),
        }
    }
}
