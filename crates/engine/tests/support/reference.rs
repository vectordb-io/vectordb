//! Deliberately independent: an append-only event list and exhaustive scans.
//! No production filter, index, segment, serialization or visibility code used.
#[derive(Default)]
pub struct Reference {
    events: Vec<(i64, i64, Option<[f64; 2]>)>,
}
impl Reference {
    pub fn put(&mut self, version: i64, key: i64, vector: Option<[f64; 2]>) {
        self.events.push((version, key, vector));
    }
    pub fn visible(&self, at: i64) -> Vec<(i64, [f64; 2])> {
        let mut keys = self
            .events
            .iter()
            .filter(|(v, _, _)| *v <= at)
            .map(|(_, k, _)| *k)
            .collect::<Vec<_>>();
        keys.sort();
        keys.dedup();
        keys.into_iter()
            .filter_map(|key| {
                self.events
                    .iter()
                    .rev()
                    .find(|(v, k, _)| *v <= at && *k == key)
                    .and_then(|(_, _, v)| v.map(|v| (key, v)))
            })
            .collect()
    }
    pub fn search(&self, at: i64, q: [f64; 2], limit: usize) -> Vec<(i64, f64)> {
        let mut hits = self
            .visible(at)
            .into_iter()
            .map(|(k, v)| (k, (q[0] - v[0]).powi(2) + (q[1] - v[1]).powi(2)))
            .collect::<Vec<_>>();
        hits.sort_by(|a, b| {
            a.1.total_cmp(&b.1)
                .then_with(|| a.0.to_string().cmp(&b.0.to_string()))
        });
        hits.truncate(limit);
        hits
    }
}
