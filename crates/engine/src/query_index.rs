//! Immutable per-bucket accelerators; only the pinned PK map owns visibility.
use crate::{
    Error, Result,
    filter::{Expr, compare},
    index::{BuiltIndex, Index},
    segment::{CollectionSpec, SegmentEngine, VectorDtype, VersionedRow},
    segment_storage::{ObjectRef, checksum},
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    collections::{BTreeMap, BTreeSet},
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, AtomicUsize, Ordering},
    },
};
pub const BUCKETS: usize = 32;
pub const INDEX_FORMAT: u32 = 1;
const MAX_OBJECT: usize = 64 * 1024 * 1024;
pub fn bucket(key: &str) -> usize {
    crate::shard::shard_for(key, BUCKETS)
}

fn identity(c: &CollectionSpec) -> String {
    checksum(
        &serde_json::to_vec(&(&c.name, &c.schema, c.dtype, &c.indexes)).expect("serializable spec"),
    )
}
fn exhausted() -> Error {
    Error::Capacity("query/index memory admission exhausted".into())
}
pub struct Budget {
    limit: usize,
    used: AtomicUsize,
}
impl Budget {
    pub fn new(limit: usize) -> Arc<Self> {
        Arc::new(Self {
            limit,
            used: AtomicUsize::new(0),
        })
    }
    pub fn acquire(self: &Arc<Self>, bytes: usize) -> Result<Reservation> {
        let mut current = self.used.load(Ordering::Acquire);
        loop {
            let next = current
                .checked_add(bytes)
                .filter(|n| *n <= self.limit)
                .ok_or_else(exhausted)?;
            match self.used.compare_exchange_weak(
                current,
                next,
                Ordering::AcqRel,
                Ordering::Acquire,
            ) {
                Ok(_) => {
                    return Ok(Reservation {
                        budget: self.clone(),
                        bytes,
                    });
                }
                Err(n) => current = n,
            }
        }
    }
    pub fn used(&self) -> usize {
        self.used.load(Ordering::Acquire)
    }
}
pub struct Reservation {
    budget: Arc<Budget>,
    bytes: usize,
}
impl Drop for Reservation {
    fn drop(&mut self) {
        self.budget.used.fetch_sub(self.bytes, Ordering::AcqRel);
    }
}
#[derive(Clone, Debug, Serialize, Deserialize)]
struct Column {
    values: Vec<(Value, Vec<usize>)>,
    nulls: Vec<usize>,
}
#[derive(Clone)]
struct Mask {
    words: Vec<u64>,
    len: usize,
}
impl Mask {
    fn new(len: usize, all: bool) -> Self {
        Self {
            words: vec![if all { u64::MAX } else { 0 }; len.div_ceil(64)],
            len,
        }
    }
    fn set(&mut self, n: usize) {
        self.words[n / 64] |= 1 << (n % 64);
    }
    fn get(&self, n: usize) -> bool {
        n < self.len && self.words[n / 64] & (1 << (n % 64)) != 0
    }
    fn indices(&self) -> impl Iterator<Item = usize> + '_ {
        (0..self.len).filter(|&n| self.get(n))
    }
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct IndexedSegment {
    format: u32,
    bucket: usize,
    revision: i64,
    identity: String,
    dtype: VectorDtype,
    specs: Vec<Index>,
    dimensions: BTreeMap<String, usize>,
    keys: Vec<(String, i64)>,
    columns: BTreeMap<String, Column>,
    ann: BTreeMap<String, BuiltIndex>,
}
impl IndexedSegment {
    fn build(
        bucket: usize,
        revision: i64,
        c: &CollectionSpec,
        rows: &[Arc<VersionedRow>],
        cancel: &AtomicBool,
    ) -> Result<Self> {
        let bodies = rows
            .iter()
            .map(|r| r.body.as_ref().unwrap().clone())
            .collect::<Vec<_>>();
        let mut columns = BTreeMap::new();
        for f in &c.schema.fields {
            if ![
                "Int8", "Int16", "Int32", "Int64", "Float", "Double", "Bool", "VarChar",
            ]
            .contains(&f.data_type.as_str())
            {
                continue;
            }
            let mut sorted = Vec::new();
            let mut nulls = vec![];
            for (n, r) in bodies.iter().enumerate() {
                let v = &r[&f.field_name];
                if v.is_null() {
                    nulls.push(n);
                } else {
                    sorted.push((v.clone(), n));
                }
            }
            sorted.sort_by(|a, b| compare(&a.0, &b.0).unwrap_or(std::cmp::Ordering::Equal));
            let mut values: Vec<(Value, Vec<usize>)> = vec![];
            for (value, n) in sorted {
                if let Some((last, positions)) = values.last_mut()
                    && compare(last, &value) == Some(std::cmp::Ordering::Equal)
                {
                    positions.push(n);
                } else {
                    values.push((value, vec![n]));
                }
            }
            columns.insert(f.field_name.clone(), Column { values, nulls });
        }
        let mut ann = BTreeMap::new();
        for index in &c.indexes {
            if cancel.load(Ordering::Acquire) {
                return Err(Error::Conflict("index build cancelled".into()));
            }
            if index.index_type != "FLAT" {
                ann.insert(
                    index.field_name.clone(),
                    BuiltIndex::build_cancellable(&bodies, index, &|| {
                        cancel.load(Ordering::Acquire)
                    })?,
                );
            }
        }
        Ok(Self {
            format: INDEX_FORMAT,
            bucket,
            revision,
            identity: identity(c),
            dtype: c.dtype,
            specs: c.indexes.clone(),
            dimensions: c
                .schema
                .fields
                .iter()
                .filter(|f| f.is_vector())
                .map(|f| (f.field_name.clone(), f.dim().unwrap_or(0)))
                .collect(),
            keys: rows.iter().map(|r| (r.key.clone(), r.version)).collect(),
            columns,
            ann,
        })
    }
    fn mask(&self, expr: &Expr) -> Mask {
        let len = self.keys.len();
        match expr {
            Expr::And(a, b) | Expr::Or(a, b) => {
                let mut a = self.mask(a);
                let b = self.mask(b);
                for (a, b) in a.words.iter_mut().zip(b.words) {
                    if matches!(expr, Expr::And(..)) {
                        *a &= b
                    } else {
                        *a |= b
                    }
                }
                a
            }
            Expr::Compare(field, op, value)
                if ["=", "==", "<", ">", "<=", ">="].contains(&op.as_str()) && !value.is_null() =>
            {
                let Some(col) = self.columns.get(field) else {
                    return Mask::new(len, true);
                };
                let mut mask = Mask::new(len, false);
                if col
                    .values
                    .first()
                    .is_some_and(|(v, _)| compare(v, value).is_none())
                {
                    return mask;
                }
                let lower = col
                    .values
                    .partition_point(|(v, _)| compare(v, value) == Some(std::cmp::Ordering::Less));
                let upper = col.values.partition_point(|(v, _)| {
                    matches!(
                        compare(v, value),
                        Some(std::cmp::Ordering::Less | std::cmp::Ordering::Equal)
                    )
                });
                let range = match op.as_str() {
                    "=" | "==" => lower..upper,
                    "<" => 0..lower,
                    "<=" => 0..upper,
                    ">" => upper..col.values.len(),
                    _ => lower..col.values.len(),
                };
                for (_, positions) in &col.values[range] {
                    for &n in positions {
                        mask.set(n);
                    }
                }
                mask
            }
            Expr::In(field, values, false) => {
                let mut mask = Mask::new(len, false);
                for v in values {
                    if !v.is_null() {
                        let next = self.mask(&Expr::Compare(field.clone(), "==".into(), v.clone()));
                        for (a, b) in mask.words.iter_mut().zip(next.words) {
                            *a |= b;
                        }
                    }
                }
                mask
            }
            Expr::Null(field, neg) => {
                let Some(col) = self.columns.get(field) else {
                    return Mask::new(len, true);
                };
                let mut mask = Mask::new(len, *neg);
                for &n in &col.nulls {
                    if *neg {
                        mask.words[n / 64] &= !(1 << (n % 64));
                    } else {
                        mask.set(n);
                    }
                }
                mask
            }
            _ => Mask::new(len, true),
        }
    }
    fn validate(&self, c: &CollectionSpec, max_revision: i64) -> Result<()> {
        let bad = || Error::Integrity("invalid persisted segment index".into());
        if self.format != INDEX_FORMAT
            || self.bucket >= BUCKETS
            || self.revision > max_revision
            || self.revision <= 0
            || self.identity != identity(c)
            || self.dtype != c.dtype
        {
            return Err(bad());
        }
        if serde_json::to_value(&self.specs)? != serde_json::to_value(&c.indexes)?
            || self.dimensions
                != c.schema
                    .fields
                    .iter()
                    .filter(|f| f.is_vector())
                    .map(|f| (f.field_name.clone(), f.dim().unwrap_or(0)))
                    .collect()
        {
            return Err(bad());
        }
        let mut seen = BTreeSet::new();
        for (key, version) in &self.keys {
            if bucket(key) != self.bucket
                || *version <= 0
                || *version > self.revision
                || !seen.insert(key)
            {
                return Err(bad());
            }
        }
        for (field, column) in &self.columns {
            let f = c.schema.field(field).map_err(|_| bad())?;
            let mut covered = vec![false; self.keys.len()];
            for &n in &column.nulls {
                if n >= covered.len() || covered[n] {
                    return Err(bad());
                }
                covered[n] = true;
            }
            let mut last = None;
            for (value, positions) in &column.values {
                f.validate(value).map_err(|_| bad())?;
                if value.is_null()
                    || last.is_some_and(|l| compare(l, value) != Some(std::cmp::Ordering::Less))
                {
                    return Err(bad());
                }
                last = Some(value);
                for &n in positions {
                    if n >= covered.len() || covered[n] {
                        return Err(bad());
                    }
                    covered[n] = true;
                }
            }
            if covered.iter().any(|v| !*v) {
                return Err(bad());
            }
        }
        for (field, index) in &self.ann {
            let spec = c
                .indexes
                .iter()
                .find(|i| &i.field_name == field)
                .ok_or_else(bad)?;
            index.validate_persisted(spec, c.schema.field(field)?, self.keys.len())?;
        }
        Ok(())
    }
}
#[derive(Clone, Debug, Serialize)]
pub struct TaskStatus {
    pub bucket: usize,
    pub revision: i64,
    pub attempt: usize,
    pub state: String,
    pub error: Option<String>,
}
struct Task {
    status: TaskStatus,
    cancel: Arc<AtomicBool>,
}
struct CachedSegment {
    segment: IndexedSegment,
    _memory: Reservation,
}
impl std::ops::Deref for CachedSegment {
    type Target = IndexedSegment;
    fn deref(&self) -> &IndexedSegment {
        &self.segment
    }
}
#[derive(Default)]
struct Cache {
    entries: BTreeMap<usize, (Arc<CachedSegment>, usize)>,
    tasks: BTreeMap<usize, Task>,
    bytes: usize,
    force_revision: i64,
    settled_revision: i64,
    paused: bool,
}
pub(crate) struct Accelerator {
    cache: Mutex<Cache>,
    limit: usize,
    resident: Arc<Budget>,
    pins: Arc<Budget>,
    scratch: Arc<Budget>,
    builds: Arc<Budget>,
    workers: AtomicUsize,
    pub built: AtomicUsize,
    pub loaded: AtomicUsize,
    pub pruned: AtomicUsize,
    pub fallback: AtomicUsize,
    pub distances: AtomicUsize,
}
impl Default for Accelerator {
    fn default() -> Self {
        Self::new(
            256 * 1024 * 1024,
            2 * 1024 * 1024 * 1024,
            128 * 1024 * 1024,
            256 * 1024 * 1024,
        )
    }
}
impl Accelerator {
    pub(crate) fn cancel_all(&self) {
        let mut cache = self.cache.lock().unwrap();
        cache.paused = true;
        for task in cache.tasks.values() {
            task.cancel.store(true, Ordering::Release);
        }
    }
    fn new(cache: usize, pins: usize, scratch: usize, builds: usize) -> Self {
        Self {
            cache: Mutex::new(Cache::default()),
            limit: cache,
            resident: Budget::new(cache),
            pins: Budget::new(pins),
            scratch: Budget::new(scratch),
            builds: Budget::new(builds),
            workers: AtomicUsize::new(0),
            built: AtomicUsize::new(0),
            loaded: AtomicUsize::new(0),
            pruned: AtomicUsize::new(0),
            fallback: AtomicUsize::new(0),
            distances: AtomicUsize::new(0),
        }
    }
    fn entries(&self, c: &CollectionSpec, revision: i64) -> BTreeMap<usize, Arc<CachedSegment>> {
        let id = identity(c);
        self.cache
            .lock()
            .unwrap()
            .entries
            .iter()
            .filter(|(_, (e, _))| e.identity == id && e.revision <= revision)
            .map(|(b, (e, _))| (*b, e.clone()))
            .collect()
    }
    fn install(&self, index: IndexedSegment, charge: usize, cancel: &AtomicBool) -> Result<()> {
        if charge > self.limit {
            return Err(exhausted());
        }
        let mut cache = self.cache.lock().unwrap();
        if cancel.load(Ordering::Acquire) {
            return Err(Error::Conflict(
                "index build cancelled before publication".into(),
            ));
        }
        if let Some((_, bytes)) = cache.entries.remove(&index.bucket) {
            cache.bytes -= bytes;
        }
        while self.resident.used().saturating_add(charge) > self.limit {
            let oldest = cache
                .entries
                .iter()
                .min_by_key(|(_, (e, _))| e.revision)
                .map(|(b, _)| *b)
                .ok_or_else(exhausted)?;
            let (_, bytes) = cache.entries.remove(&oldest).unwrap();
            cache.bytes -= bytes;
        }
        let memory = self.resident.acquire(charge)?;
        cache.bytes += charge;
        cache.entries.insert(
            index.bucket,
            (
                Arc::new(CachedSegment {
                    segment: index,
                    _memory: memory,
                }),
                charge,
            ),
        );
        Ok(())
    }
}
pub struct ReadView {
    engine: SegmentEngine,
    _pin: Reservation,
}
impl ReadView {
    pub fn execute(&self, op: &str, r: Value) -> Result<Value> {
        self.engine.execute(op, r)
    }
    pub fn revision(&self) -> i64 {
        self.engine.revision()
    }
}
pub struct IndexJob {
    bucket: usize,
    revision: i64,
    spec: CollectionSpec,
    rows: Vec<Arc<VersionedRow>>,
    cache: Arc<Accelerator>,
    cancel: Arc<AtomicBool>,
    _memory: Reservation,
}
impl IndexJob {
    pub fn run(self) {
        let outcome = IndexedSegment::build(
            self.bucket,
            self.revision,
            &self.spec,
            &self.rows,
            &self.cancel,
        )
        .and_then(|index| {
            if self.cancel.load(Ordering::Acquire) {
                return Err(Error::Conflict("cancelled".into()));
            }
            let bytes = serde_json::to_vec(&index)?;
            if bytes.len() > MAX_OBJECT {
                return Err(exhausted());
            }
            self.cache
                .install(index, bytes.len().saturating_mul(4), &self.cancel)
        });
        let mut cache = self.cache.cache.lock().unwrap();
        if let Some(task) = cache.tasks.get_mut(&self.bucket) {
            task.status.state = if self.cancel.load(Ordering::Acquire) {
                "cancelled"
            } else if outcome.is_ok() {
                "ready"
            } else {
                "failed"
            }
            .into();
            task.status.error = outcome.err().map(|e| e.to_string());
        }
        if !self.cancel.load(Ordering::Acquire)
            && cache
                .tasks
                .get(&self.bucket)
                .is_some_and(|t| t.status.state == "ready")
        {
            self.cache.built.fetch_add(1, Ordering::Relaxed);
        }
    }
}
impl Drop for IndexJob {
    fn drop(&mut self) {
        let mut c = self.cache.cache.lock().unwrap();
        if let Some(t) = c.tasks.get_mut(&self.bucket)
            && ["running", "cancelling"].contains(&t.status.state.as_str())
        {
            t.status.state = "failed".into();
            t.status.error = Some("worker dropped before completion".into());
        }
        // Publish the terminal task state before making this worker slot available.
        self.cache.workers.fetch_sub(1, Ordering::AcqRel);
    }
}
impl SegmentEngine {
    pub fn with_query_limits(cache: usize, pins: usize, scratch: usize, builds: usize) -> Self {
        Self {
            accelerator: Arc::new(Accelerator::new(cache, pins, scratch, builds)),
            ..Self::new()
        }
    }
    pub fn read_view(&self) -> Result<ReadView> {
        let pin = self
            .accelerator
            .pins
            .acquire(self.bytes.saturating_mul(4).max(1024))?;
        Ok(ReadView {
            engine: self.clone(),
            _pin: pin,
        })
    }
    pub(crate) fn query_memory(&self, r: &Value) -> Result<Reservation> {
        let query_bytes = serde_json::to_vec(r)?.len();
        self.accelerator.scratch.acquire(
            self.rows
                .len()
                .saturating_mul(160)
                .saturating_add(query_bytes.saturating_mul(4))
                .max(1024),
        )
    }
    pub fn request_index_build(&self) {
        let mut c = self.accelerator.cache.lock().unwrap();
        c.paused = false;
        c.force_revision = self.revision;
        c.settled_revision = -1;
        for task in c.tasks.values_mut() {
            if ["failed", "cancelled", "blocked"].contains(&task.status.state.as_str()) {
                task.status.attempt = 0;
                task.status.state = "retrying".into();
            }
        }
    }
    pub fn cancel_index_builds(&self) {
        let mut c = self.accelerator.cache.lock().unwrap();
        c.paused = true;
        c.force_revision = 0;
        for task in c.tasks.values_mut() {
            task.cancel.store(true, Ordering::Release);
            if task.status.state == "running" {
                task.status.state = "cancelling".into();
            }
        }
    }
    pub fn index_status(&self) -> Value {
        let c = self.accelerator.cache.lock().unwrap();
        json!({"tasks":c.tasks.values().map(|t|t.status.clone()).collect::<Vec<_>>(),"paused":c.paused,"cacheBytes":c.bytes,"cacheLimit":self.accelerator.limit,"residentIndexBytes":self.accelerator.resident.used(),"builtSegments":self.accelerator.built.load(Ordering::Relaxed),"loadedSegments":self.accelerator.loaded.load(Ordering::Relaxed),"prunedSegments":self.accelerator.pruned.load(Ordering::Relaxed),"flatFallbacks":self.accelerator.fallback.load(Ordering::Relaxed),"distanceEvaluations":self.accelerator.distances.load(Ordering::Relaxed),"pinnedBytes":self.accelerator.pins.used(),"scratchBytes":self.accelerator.scratch.used(),"buildBytes":self.accelerator.builds.used()})
    }
    pub fn index_jobs(&self) -> Vec<IndexJob> {
        let Some(spec) = self.spec.as_ref() else {
            return vec![];
        };
        if self.accelerator.workers.load(Ordering::Acquire) >= 2 {
            return vec![];
        }
        {
            let cache = self.accelerator.cache.lock().unwrap();
            if cache.paused || cache.settled_revision == self.revision {
                return vec![];
            }
        }
        let id = identity(spec);
        let mut buckets: Vec<Vec<Arc<VersionedRow>>> = vec![vec![]; BUCKETS];
        for row in self.rows.values() {
            if row.body.is_some() {
                buckets[bucket(&row.key)].push(row.clone());
            }
        }
        let mut cache = self.accelerator.cache.lock().unwrap();
        let mut jobs = vec![];
        for (bucket, rows) in buckets.into_iter().enumerate() {
            if rows.is_empty() {
                continue;
            }
            if self.accelerator.workers.load(Ordering::Acquire) >= 2 {
                break;
            }
            if let Some(t) = cache.tasks.get(&bucket) {
                if ["running", "cancelling"].contains(&t.status.state.as_str()) {
                    continue;
                }
                if t.status.revision == self.revision
                    && (t.status.state == "cancelled"
                        || t.status.state == "ready"
                        || t.status.attempt >= 3)
                {
                    continue;
                }
            }
            let existing = cache.entries.get(&bucket).map(|(e, _)| e);
            let needed = existing.is_none_or(|e| {
                e.identity != id
                    || e.revision < cache.force_revision
                    || rows.iter().filter(|r| r.version > e.revision).count() >= 256
            });
            if !needed {
                continue;
            }
            let m = spec
                .indexes
                .iter()
                .map(|i| i.params["M"].as_u64().unwrap_or(16) as usize)
                .max()
                .unwrap_or(16);
            let estimate = rows
                .iter()
                .map(|r| {
                    r.body.as_ref().unwrap().to_string().len().saturating_mul(4) + m * 17 * 16 + 256
                })
                .sum();
            let Ok(memory) = self.accelerator.builds.acquire(estimate) else {
                if self.accelerator.workers.load(Ordering::Acquire) == 0 {
                    cache.tasks.insert(
                        bucket,
                        Task {
                            status: TaskStatus {
                                bucket,
                                revision: self.revision,
                                attempt: 3,
                                state: "blocked".into(),
                                error: Some("index build memory quota".into()),
                            },
                            cancel: Arc::new(AtomicBool::new(false)),
                        },
                    );
                }
                continue;
            };
            let attempt = cache.tasks.get(&bucket).map_or(1, |t| {
                if t.status.revision == self.revision {
                    t.status.attempt + 1
                } else {
                    1
                }
            });
            let cancel = Arc::new(AtomicBool::new(false));
            cache.tasks.insert(
                bucket,
                Task {
                    status: TaskStatus {
                        bucket,
                        revision: self.revision,
                        attempt,
                        state: "running".into(),
                        error: None,
                    },
                    cancel: cancel.clone(),
                },
            );
            self.accelerator.workers.fetch_add(1, Ordering::AcqRel);
            jobs.push(IndexJob {
                bucket,
                revision: self.revision,
                spec: spec.clone(),
                rows,
                cache: self.accelerator.clone(),
                cancel,
                _memory: memory,
            });
        }
        if jobs.is_empty() && self.accelerator.workers.load(Ordering::Acquire) == 0 {
            cache.settled_revision = self.revision;
        }
        jobs
    }
    pub fn build_indexes(&self) -> Result<()> {
        self.request_index_build();
        loop {
            let jobs = self.index_jobs();
            if jobs.is_empty() {
                break;
            }
            for job in jobs {
                job.run();
            }
        }
        let status = self.index_status();
        if status["tasks"].as_array().unwrap().iter().any(|t| {
            ["failed", "blocked", "running", "cancelling", "cancelled"]
                .iter()
                .any(|state| t["state"] == *state)
        }) {
            return Err(exhausted());
        }
        Ok(())
    }
    pub(crate) fn indexed_rows(
        &self,
        expr: &Expr,
        partitions: &[String],
        query: Option<(&Value, &Index, usize, usize, usize, bool)>,
    ) -> Vec<Arc<VersionedRow>> {
        let Some(c) = &self.spec else { return vec![] };
        let entries = self.accelerator.entries(c, self.revision);
        let mut out = vec![];
        let visible = |key: &str, version: i64| {
            self.rows
                .get(key)
                .filter(|r| {
                    r.version == version
                        && r.body.as_ref().is_some_and(|body| {
                            (partitions.is_empty() || partitions.contains(&r.partition))
                                && expr.matches(body)
                        })
                })
                .cloned()
        };
        for segment in entries.values() {
            let mask = segment.mask(expr);
            let selected = mask
                .indices()
                .filter_map(|n| {
                    let (k, v) = &segment.keys[n];
                    visible(k, *v).map(|r| (n, r))
                })
                .collect::<Vec<_>>();
            if selected.is_empty() {
                self.accelerator.pruned.fetch_add(1, Ordering::Relaxed);
                continue;
            }
            if let Some((q, index, nprobe, ef, k, force)) = query
                && let Some(ann) = segment.ann.get(&index.field_name)
            {
                let strict = selected.len() <= k.saturating_mul(4).max(segment.keys.len() / 10);
                if !force && !strict {
                    let ids: BTreeSet<_> = ann
                        .candidates(q, nprobe, ef.max(k.saturating_mul(2)))
                        .into_iter()
                        .collect();
                    let hits = selected
                        .iter()
                        .filter(|(n, _)| ids.contains(n))
                        .map(|(_, r)| r.clone())
                        .collect::<Vec<_>>();
                    if hits.len() >= k.min(selected.len()) {
                        out.extend(hits);
                        continue;
                    }
                }
                self.accelerator.fallback.fetch_add(1, Ordering::Relaxed);
            }
            out.extend(selected.into_iter().map(|(_, r)| r));
        }
        for row in self.rows.values() {
            if entries
                .get(&bucket(&row.key))
                .is_some_and(|e| row.version <= e.revision)
            {
                continue;
            }
            if let Some(r) = visible(&row.key, row.version) {
                out.push(r);
            }
        }
        out.sort_by(|a, b| a.key.cmp(&b.key));
        out
    }
    pub(crate) fn persist_indexes(&self) -> Result<Vec<(String, Vec<u8>)>> {
        let Some(c) = &self.spec else {
            return Ok(vec![]);
        };
        self.accelerator
            .entries(c, self.revision)
            .values()
            .map(|e| {
                Ok((
                    format!("index-bucket-{}.json", e.bucket),
                    serde_json::to_vec(&e.segment)?,
                ))
            })
            .collect()
    }
    pub(crate) fn load_index(&self, reference: &ObjectRef, bytes: Vec<u8>) -> Result<()> {
        if bytes.len() != reference.bytes
            || bytes.len() > MAX_OBJECT
            || checksum(&bytes) != reference.sha256
        {
            return Err(Error::Integrity("index checksum/length mismatch".into()));
        }
        let index: IndexedSegment = serde_json::from_slice(&bytes)?;
        let spec = self
            .spec
            .as_ref()
            .ok_or_else(|| Error::Integrity("index without collection".into()))?;
        index.validate(spec, self.revision)?;
        if reference.key != format!("index-bucket-{}.json", index.bucket) {
            return Err(Error::Integrity("index object identity mismatch".into()));
        }
        let keys: BTreeMap<_, _> = index.keys.iter().map(|(k, v)| (k.as_str(), *v)).collect();
        if self.rows.values().any(|r| {
            r.body.is_some()
                && bucket(&r.key) == index.bucket
                && r.version <= index.revision
                && keys.get(r.key.as_str()) != Some(&r.version)
        }) {
            return Err(Error::Integrity(
                "index coverage omits a visible version".into(),
            ));
        }
        let charge = bytes.len().saturating_mul(4);
        if charge <= self.accelerator.limit {
            self.accelerator
                .install(index, charge, &AtomicBool::new(false))?;
            self.accelerator.loaded.fetch_add(1, Ordering::Relaxed);
        }
        Ok(())
    }
}
