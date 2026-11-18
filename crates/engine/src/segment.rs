//! Native single-collection state. A prepared batch contains only touched keys.
use crate::{
    Error, Result,
    error::invalid,
    filter::{Expr, compare, lookup},
    index::{Index, ascending, score},
    schema::{Schema, name, number},
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::BTreeSet;
use std::sync::Arc;

pub const STATE_FORMAT: u32 = 2;
pub const MAX_METADATA_BYTES: usize = 1024 * 1024;
pub const MAX_STATE_BYTES: usize = 256 * 1024 * 1024;
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub enum VectorDtype {
    #[default]
    Float32,
    Float64,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CollectionSpec {
    pub format: u32,
    pub name: String,
    pub schema: Schema,
    pub dtype: VectorDtype,
    pub indexes: Vec<Index>,
    pub next_id: i64,
    pub partitions: BTreeSet<String>,
    #[serde(default = "crate::shard::one")]
    pub shard_count: usize,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VersionedRow {
    pub key: String,
    pub partition: String,
    pub version: i64,
    pub body: Option<Value>,
}
impl VersionedRow {
    fn bytes(&self) -> usize {
        self.key.len()
            + self.partition.len()
            + self.body.as_ref().map_or(0, |v| v.to_string().len())
            + 128
    }
}
#[derive(Debug)]
pub struct MutationBatch {
    pub(crate) base: i64,
    pub(crate) spec: Option<CollectionSpec>,
    pub(crate) rows: Vec<VersionedRow>,
    pub result: Value,
}
#[derive(Clone, Default)]
pub struct SegmentEngine {
    pub(crate) spec: Option<CollectionSpec>,
    pub(crate) selection: Option<BTreeSet<usize>>,
    pub(crate) rows: im::OrdMap<String, Arc<VersionedRow>>,
    pub(crate) revision: i64,
    pub(crate) bytes: usize,
    pub(crate) accelerator: Arc<crate::query_index::Accelerator>,
}
impl SegmentEngine {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn revision(&self) -> i64 {
        self.revision
    }
    pub fn get_version(&self, key: &Value) -> Option<VersionedRow> {
        self.rows.get(&key.to_string()).map(|r| r.as_ref().clone())
    }
    pub fn spec(&self) -> Option<&CollectionSpec> {
        self.spec.as_ref()
    }
    pub(crate) fn recount_bytes(&mut self) {
        self.bytes = self.rows.values().map(|r| r.bytes()).sum();
    }
    pub fn state_bytes(&self) -> usize {
        self.bytes
    }
    pub fn row_count(&self) -> usize {
        self.rows.values().filter(|r| r.body.is_some()).count()
    }
    /// Logical backup rows retain the business partition and canonical primary key.
    pub fn live_rows(&self) -> impl Iterator<Item = &VersionedRow> {
        self.rows
            .values()
            .map(AsRef::as_ref)
            .filter(|r| r.body.is_some())
    }
    /// Materialize a verified logical snapshot at the new log's bootstrap offset.
    pub fn from_backup(
        spec: Option<CollectionSpec>,
        rows: Vec<VersionedRow>,
        revision: i64,
    ) -> Result<Self> {
        let mut engine = Self {
            revision,
            spec,
            ..Default::default()
        };
        if revision <= 0 {
            return Err(invalid("bootstrap revision must be positive"));
        }
        if let Some(spec) = engine.spec.as_ref() {
            Schema::from_request(&json!({"schema":spec.schema}))?;
            crate::shard::validate_count(spec.shard_count)?;
            if spec.format != STATE_FORMAT
                || spec.next_id < 1
                || !spec.partitions.contains("_default")
            {
                return Err(invalid("invalid backup collection metadata"));
            }
            for index in &spec.indexes {
                index
                    .clone()
                    .validate(spec.schema.field(&index.field_name)?)?;
            }
        }
        for mut row in rows {
            let spec = engine
                .spec
                .as_ref()
                .ok_or_else(|| invalid("backup rows without collection"))?;
            let body = row
                .body
                .as_mut()
                .ok_or_else(|| invalid("tombstone in logical backup"))?;
            spec.schema.validate_row(body)?;
            normalize_vectors(spec, body)?;
            let pk = &body[&spec.schema.primary().field_name];
            let canonical = pk.to_string();
            if canonical != row.key
                || !spec.partitions.contains(&row.partition)
                || (spec.schema.auto_id && pk.as_i64().is_some_and(|n| n >= spec.next_id))
            {
                return Err(invalid(
                    "invalid backup primary key, partition or ID watermark",
                ));
            }
            row.version = revision;
            engine.bytes += row.bytes();
            if engine.bytes > MAX_STATE_BYTES
                || engine.rows.insert(row.key.clone(), Arc::new(row)).is_some()
            {
                return Err(invalid("duplicate backup key or state quota exceeded"));
            }
        }
        Ok(engine)
    }
    pub fn prepare(&self, operation: &str, request: Value) -> Result<MutationBatch> {
        self.prepare_with_limit(operation, request, MAX_STATE_BYTES)
    }
    pub fn prepare_with_limit(
        &self,
        operation: &str,
        request: Value,
        limit: usize,
    ) -> Result<MutationBatch> {
        let mut r = request;
        let shard_count = if operation == "collections/create" {
            let value = r
                .as_object_mut()
                .and_then(|r| r.remove("shardCount"))
                .unwrap_or(json!(1));
            let count = value
                .as_u64()
                .ok_or_else(|| invalid("shardCount must be an integer"))?
                as usize;
            crate::shard::validate_count(count)?;
            count
        } else {
            1
        };
        let dtype = if operation == "collections/create" {
            match r.as_object_mut().and_then(|r| r.remove("vectorDtype")) {
                None => VectorDtype::Float32,
                Some(v) => serde_json::from_value(v)
                    .map_err(|_| invalid("vectorDtype must be Float32 or Float64"))?,
            }
        } else {
            VectorDtype::Float32
        };
        if operation == "entities/delete" {
            keys(&r, &["collectionName", "dbName", "ids"], "delete")?;
        } else {
            crate::engine::validate_request(operation, &r)?;
        }
        if r.get("dbName").is_some_and(|v| v != "default") {
            return Err(invalid("native engine requires default database"));
        }
        let mut c = if operation == "collections/create" {
            if self.spec.is_some() {
                return Err(Error::Conflict("collection already exists".into()));
            }
            let n = text(&r, "collectionName")?;
            name(n)?;
            let schema = Schema::from_request(&r)?;
            let mut c = CollectionSpec {
                format: STATE_FORMAT,
                name: n.into(),
                schema,
                dtype,
                indexes: vec![],
                next_id: 1,
                partitions: BTreeSet::from(["_default".into()]),
                shard_count,
            };
            if r.get("indexParams").is_some() {
                self.add_indexes(&mut c, &r["indexParams"])?;
            } else {
                for f in &c.schema.fields {
                    if f.is_vector() {
                        let metric =
                            r["metricType"]
                                .as_str()
                                .unwrap_or(match f.data_type.as_str() {
                                    "SparseFloatVector" => "IP",
                                    "BinaryVector" => "HAMMING",
                                    _ => "COSINE",
                                });
                        let mut i: Index = serde_json::from_value(
                            json!({"fieldName":f.field_name,"metricType":metric}),
                        )?;
                        i.validate(f)?;
                        c.indexes.push(i);
                    }
                }
            }
            c
        } else {
            self.collection(&r)?.clone()
        };
        let mut rows = vec![];
        let mut result = json!({});
        match operation {
            "collections/create" => {}
            "collections/drop" => {
                return Ok(MutationBatch {
                    base: self.revision,
                    spec: None,
                    rows: vec![],
                    result,
                });
            }
            "partitions/drop" => {
                let names = strings(&r, "partitionNames")?;
                if names.is_empty() || names.iter().any(|p| p == "_default") {
                    return Err(invalid(
                        "cannot drop default partition; provide partitionNames",
                    ));
                }
                for p in &names {
                    self.partition(&c, p)?;
                }
                for p in &names {
                    c.partitions.remove(p);
                }
                rows.extend(
                    self.rows
                        .values()
                        .filter(|row| names.contains(&row.partition))
                        .map(|row| VersionedRow {
                            key: row.key.clone(),
                            partition: "_default".into(),
                            version: 0,
                            body: None,
                        }),
                );
            }
            "partitions/create" => {
                let p = text(&r, "partitionName")?;
                name(p)?;
                if !c.partitions.insert(p.into()) {
                    return Err(Error::Conflict("partition already exists".into()));
                }
            }
            "indexes/create" => self.add_indexes(&mut c, &r["indexParams"])?,
            "indexes/drop" => {
                let n = text(&r, "indexName")?;
                let len = c.indexes.len();
                c.indexes.retain(|i| i.index_name != n);
                if len == c.indexes.len() {
                    return Err(Error::NotFound("index not found".into()));
                }
            }
            "entities/insert" | "entities/upsert" => {
                let upsert = operation.ends_with("upsert");
                let data = r["data"]
                    .as_array()
                    .ok_or_else(|| invalid("data must be array"))?;
                if data.is_empty() || data.len() > 10000 {
                    return Err(invalid("write batch must have 1..10000 entities"));
                }
                let p = r["partitionName"].as_str().unwrap_or("_default");
                self.partition(&c, p)?;
                let pk = &c.schema.primary().field_name;
                let mut ids = vec![];
                let mut seen = BTreeSet::new();
                for input in data {
                    let mut row = input.clone();
                    if !row.is_object() {
                        return Err(invalid("entity must be object"));
                    }
                    if c.schema.auto_id && !upsert {
                        if row.get(pk).is_some() {
                            return Err(invalid("do not supply primary key with autoID insert"));
                        }
                        row[pk] = json!(c.next_id);
                        c.next_id = c
                            .next_id
                            .checked_add(1)
                            .ok_or_else(|| invalid("autoID exhausted"))?;
                    }
                    c.schema.validate_row(&mut row)?;
                    normalize_vectors(&c, &mut row)?;
                    let id = row[pk].clone();
                    let key = id.to_string();
                    if !seen.insert(key.clone()) {
                        return Err(invalid("duplicate primary key within batch"));
                    }
                    if !upsert && self.rows.get(&key).is_some_and(|r| r.body.is_some()) {
                        return Err(Error::Conflict("primary key already exists".into()));
                    }
                    if c.schema.auto_id && upsert && id.as_i64().unwrap() >= c.next_id {
                        c.next_id = id
                            .as_i64()
                            .unwrap()
                            .checked_add(1)
                            .ok_or_else(|| invalid("autoID exhausted"))?;
                    }
                    ids.push(id);
                    rows.push(VersionedRow {
                        key,
                        partition: p.into(),
                        version: 0,
                        body: Some(row),
                    });
                }
                result = if upsert {
                    json!({"upsertCount":ids.len(),"upsertIds":ids})
                } else {
                    json!({"insertCount":ids.len(),"insertIds":ids})
                };
            }
            "entities/delete" => {
                let ids = r["ids"]
                    .as_array()
                    .ok_or_else(|| invalid("delete requires explicit ids"))?;
                if ids.is_empty() || ids.len() > 10000 {
                    return Err(invalid("delete requires 1..10000 ids"));
                }
                let mut seen = BTreeSet::new();
                let mut count = 0;
                for id in ids {
                    c.schema.primary().validate(id)?;
                    let key = id.to_string();
                    if !seen.insert(key.clone()) {
                        return Err(invalid("duplicate delete id"));
                    }
                    if let Some(old) = self.rows.get(&key).filter(|r| r.body.is_some()) {
                        count += 1;
                        rows.push(VersionedRow {
                            key,
                            partition: old.partition.clone(),
                            version: 0,
                            body: None,
                        });
                    }
                }
                result = json!({"deleteCount":count});
            }
            _ => return Err(Error::Unsupported(operation.into())),
        }
        let mut bytes = self.bytes;
        for row in &rows {
            bytes = bytes.saturating_sub(self.rows.get(&row.key).map_or(0, |r| r.bytes()))
                + row.bytes();
        }
        let metadata_bytes = serde_json::to_vec(&c)?.len();
        if metadata_bytes > MAX_METADATA_BYTES {
            return Err(Error::Conflict(
                "collection metadata exceeds 1 MiB quota".into(),
            ));
        }
        if limit < MAX_STATE_BYTES
            && bytes + metadata_bytes > limit
            && matches!(
                operation,
                "collections/create" | "entities/insert" | "entities/upsert"
            )
        {
            return Err(Error::Conflict("collection storage quota reached".into()));
        }
        if bytes + metadata_bytes > MAX_STATE_BYTES {
            return Err(Error::Conflict(
                "current state exceeds 256 MiB quota; delete data or increase deployment capacity"
                    .into(),
            ));
        }
        Ok(MutationBatch {
            base: self.revision,
            spec: Some(c),
            rows,
            result,
        })
    }
    pub fn commit(&mut self, batch: MutationBatch, version: i64) -> Result<Value> {
        if batch.base != self.revision || version <= self.revision {
            return Err(Error::Conflict("stale prepared batch or version".into()));
        }
        self.spec = batch.spec;
        if self.spec.is_none() {
            self.rows.clear();
            self.bytes = 0;
            self.accelerator.cancel_all();
            self.accelerator = Arc::new(crate::query_index::Accelerator::default());
        }
        for mut row in batch.rows {
            self.bytes = self
                .bytes
                .saturating_sub(self.rows.get(&row.key).map_or(0, |r| r.bytes()))
                + row.bytes();
            row.version = version;
            self.rows.insert(row.key.clone(), Arc::new(row));
        }
        self.revision = version;

        Ok(batch.result)
    }
    /// Safe only after a full state snapshot has been persisted. No historical reads
    /// are served by this engine; existing snapshot readers own immutable files.
    pub fn reclaim_tombstones(&mut self) {
        self.rows = self
            .rows
            .iter()
            .filter(|(_, r)| r.body.is_some())
            .map(|(k, r)| (k.clone(), r.clone()))
            .collect();
        self.bytes = self.rows.values().map(|r| r.bytes()).sum();
    }
    fn collection(&self, r: &Value) -> Result<&CollectionSpec> {
        self.spec
            .as_ref()
            .filter(|c| r["collectionName"] == c.name)
            .ok_or_else(|| Error::NotFound("collection not found".into()))
    }
    pub fn execute(&self, op: &str, r: Value) -> Result<Value> {
        crate::engine::validate_request(op, &r)?;
        let _memory = self.query_memory(&r)?;
        if r.get("dbName").is_some_and(|v| v != "default") {
            return Err(invalid("native engine requires default database"));
        }
        if op == "collections/has" {
            return Ok(json!({"has":self.collection(&r).is_ok()}));
        }
        let c = self.collection(&r)?.clone();
        match op {
            "collections/describe" => Ok(
                json!({"collectionName":c.name,"schema":c.schema,"indexes":c.indexes,"vectorDtype":c.dtype,"loadState":"LoadStateLoaded"}),
            ),
            "collections/get_stats" => Ok(json!({"rowCount":self.row_count()})),
            "collections/get_load_state" => {
                Ok(json!({"loadState":"LoadStateLoaded","loadProgress":100}))
            }
            "partitions/list" => Ok(json!(c.partitions)),
            "partitions/has" => Ok(json!({"has":c.partitions.contains(text(&r,"partitionName")?)})),
            "partitions/get_stats" => {
                let p = text(&r, "partitionName")?;
                self.partition(&c, p)?;
                Ok(
                    json!({"rowCount":self.rows.values().filter(|r|r.partition==p&&r.body.is_some()).count()}),
                )
            }
            "indexes/list" => Ok(json!(
                c.indexes.iter().map(|i| &i.index_name).collect::<Vec<_>>()
            )),
            "indexes/describe" => {
                let n = text(&r, "indexName")?;
                Ok(json!([c
                    .indexes
                    .iter()
                    .find(|i| i.index_name == n)
                    .ok_or_else(|| Error::NotFound(
                        "index not found".into()
                    ))?]))
            }
            "entities/count" => {
                let mut request = r.clone();
                request["outputFields"] = json!(["count(*)"]);
                let result = self.query(&c, &request, false)?;
                Ok(json!({"count":result[0]["count(*)"]}))
            }
            "entities/get" | "entities/query" => self.query(&c, &r, op.ends_with("get")),
            "entities/search" => self.search(&c, &r),
            "entities/hybrid_search" => {
                let plan = crate::distributed_query::QueryPlan::new(c, op, r)?;
                let values = plan
                    .requests
                    .iter()
                    .map(|request| self.execute(&plan.operation, request.clone()))
                    .collect::<Result<Vec<_>>>()?;
                plan.merge(vec![values])
            }
            _ => Err(Error::Unsupported(op.into())),
        }
    }
    fn partition(&self, c: &CollectionSpec, p: &str) -> Result<()> {
        if c.partitions.contains(p) {
            Ok(())
        } else {
            Err(Error::NotFound("partition not found".into()))
        }
    }
    fn add_indexes(&self, c: &mut CollectionSpec, v: &Value) -> Result<()> {
        let indexes: Vec<Index> =
            serde_json::from_value(v.clone()).map_err(|e| invalid(e.to_string()))?;
        if indexes.is_empty() {
            return Err(invalid("indexParams cannot be empty"));
        }
        for mut i in indexes {
            i.validate(c.schema.field(&i.field_name)?)?;
            if c.indexes
                .iter()
                .any(|old| old.field_name == i.field_name || old.index_name == i.index_name)
            {
                return Err(Error::Conflict(
                    "index exists; drop it before replacing".into(),
                ));
            }
            c.indexes.push(i);
        }
        Ok(())
    }
    fn filter(&self, c: &CollectionSpec, r: &Value) -> Result<Expr> {
        let s = match r.get("filter") {
            None => "",
            Some(v) => v
                .as_str()
                .ok_or_else(|| invalid("filter must be a string"))?,
        };
        let expr = Expr::parse(s)?;
        let mut fields = Vec::new();
        expr.fields(&mut fields);
        if !c.schema.enable_dynamic_field {
            for f in fields {
                c.schema.field(f.split('.').next().unwrap())?;
            }
        }
        Ok(expr)
    }
    fn query(&self, c: &CollectionSpec, r: &Value, get: bool) -> Result<Value> {
        let expr = self.filter(c, r)?;
        let fields = output_fields(c, r)?;
        let partitions = strings(r, "partitionNames")?;
        for p in &partitions {
            self.partition(c, p)?;
        }
        let ids = if get {
            let v = r.get("id").ok_or_else(|| invalid("id is required"))?;
            let ids = if let Some(a) = v.as_array() {
                a.clone()
            } else {
                vec![v.clone()]
            };
            for id in &ids {
                c.schema.primary().validate(id)?;
            }
            Some(ids)
        } else {
            None
        };
        let selected = if let Some(ids) = ids {
            ids.iter()
                .map(Value::to_string)
                .collect::<BTreeSet<_>>()
                .iter()
                .filter_map(|key| self.rows.get(key))
                .filter(|row| {
                    row.body.is_some()
                        && (partitions.is_empty() || partitions.contains(&row.partition))
                })
                .cloned()
                .collect()
        } else {
            self.indexed_rows(&expr, &partitions, None)
        };
        let mut rows = selected
            .iter()
            .filter_map(|row| row.body.as_ref())
            .collect::<Vec<_>>();
        if fields == ["count(*)"] {
            return Ok(json!([{"count(*)":rows.len()}]));
        }
        let order = strings(r, "orderByFields")?;
        for field in &order {
            let n = field.split(':').next().unwrap();
            let f = c.schema.field(n)?;
            if f.is_vector() || ["JSON", "Array"].contains(&f.data_type.as_str()) {
                return Err(invalid("ordering requires scalar fields"));
            }
            if field
                .split_once(':')
                .is_some_and(|(_, direction)| !["asc", "desc"].contains(&direction))
            {
                return Err(invalid("order direction must be asc or desc"));
            }
        }
        if !order.is_empty() {
            rows.sort_by(|a, b| {
                for o in &order {
                    let n = o.split(':').next().unwrap();
                    let mut cmp = compare(&a[n], &b[n])
                        .unwrap_or_else(|| a[n].is_null().cmp(&b[n].is_null()));
                    if o.ends_with(":desc") {
                        cmp = cmp.reverse();
                    }
                    if !cmp.is_eq() {
                        return cmp;
                    }
                }
                std::cmp::Ordering::Equal
            });
        }
        let (limit, offset) = pagination(r)?;
        Ok(json!(
            rows.iter()
                .skip(offset)
                .take(limit)
                .map(|row| project(row, &fields, &c.schema))
                .collect::<Vec<_>>()
        ))
    }
    fn search(&self, c: &CollectionSpec, r: &Value) -> Result<Value> {
        let expr = self.filter(c, r)?;
        let fields = output_fields(c, r)?;
        if fields.contains(&"count(*)".into()) {
            return Err(invalid("count(*) is only supported by query"));
        }
        let vectors: Vec<_> = c.schema.fields.iter().filter(|f| f.is_vector()).collect();
        let field = match r["annsField"].as_str() {
            Some(n) => c.schema.field(n)?,
            None if vectors.len() == 1 => vectors[0],
            _ => {
                return Err(invalid(
                    "annsField is required for multi-vector collections",
                ));
            }
        };
        let index = c
            .indexes
            .iter()
            .find(|i| i.field_name == field.field_name)
            .ok_or_else(|| Error::Conflict("vector field has no index".into()))?
            .clone();
        let mut data = r["data"]
            .as_array()
            .ok_or_else(|| invalid("search data must be an array of vectors"))?
            .clone();
        if data.is_empty() || data.len() > 128 {
            return Err(invalid("search requires 1..128 query vectors"));
        }
        for q in &mut data {
            field.validate(q)?;
            if field.data_type == "FloatVector" && c.dtype == VectorDtype::Float32 {
                for v in q.as_array_mut().unwrap() {
                    let x = v.as_f64().unwrap() as f32;
                    if !x.is_finite() {
                        return Err(invalid("Float32 query overflow"));
                    }
                    *v = json!(f64::from(x));
                }
            }
        }
        let sp = &r["searchParams"];
        if !sp.is_null() {
            keys(sp, &["metricType", "params"], "searchParams")?;
        }
        if let Some(metric) = sp.get("metricType")
            && metric.as_str() != Some(&index.metric_type)
        {
            return Err(invalid("search metric must match index metric"));
        }
        let params = &sp["params"];
        if !params.is_null() {
            keys(
                params,
                &["nprobe", "ef", "radius", "range_filter"],
                "search params",
            )?;
        }
        let nprobe = number(&params["nprobe"], 8, 4096, "nprobe")?;
        if nprobe == 0 {
            return Err(invalid("nprobe must be positive"));
        }
        if index.index_type != "IVF_FLAT" && params.get("nprobe").is_some() {
            return Err(invalid("nprobe only applies to IVF_FLAT"));
        }
        let ef = number(&params["ef"], 128, 65536, "ef")?;
        if ef == 0 || (params.get("ef").is_some() && index.index_type != "HNSW") {
            return Err(invalid("ef must be positive and applies only to HNSW"));
        }
        let radius = optional_f64(params, "radius")?;
        let range = optional_f64(params, "range_filter")?;
        if range.is_some() && radius.is_none() {
            return Err(invalid("range_filter requires radius"));
        }
        if let (Some(lo), Some(hi)) = (range, radius)
            && ((ascending(&index.metric_type) && lo >= hi)
                || (!ascending(&index.metric_type) && lo <= hi))
        {
            return Err(invalid("invalid range bounds for metric"));
        }
        let group = match r.get("groupingField") {
            Some(v) => Some(
                v.as_str()
                    .ok_or_else(|| invalid("groupingField must be a string"))?,
            ),
            None => None,
        };
        if let Some(g) = group {
            let f = c.schema.field(g);
            if f.is_err() && !c.schema.enable_dynamic_field {
                return Err(invalid("unknown grouping field"));
            }
            if f.is_ok_and(|f| f.is_vector() || ["JSON", "Array"].contains(&f.data_type.as_str())) {
                return Err(invalid("grouping requires scalar field"));
            }
        }
        let (limit, offset) = pagination(r)?;
        let partitions = strings(r, "partitionNames")?;
        for p in &partitions {
            self.partition(c, p)?;
        }
        let mut out = Vec::new();
        for (qi, q) in data.iter().enumerate() {
            let selected = self.indexed_rows(
                &expr,
                &partitions,
                Some((
                    q,
                    &index,
                    nprobe,
                    ef,
                    limit + offset,
                    group.is_some() || radius.is_some(),
                )),
            );
            let rows = selected
                .iter()
                .map(|row| row.body.as_ref().unwrap())
                .collect::<Vec<_>>();
            if let Some(g) = group
                && rows
                    .iter()
                    .any(|row| row[g].is_array() || row[g].is_object())
            {
                return Err(invalid("grouping requires scalar field"));
            }
            let candidates = 0..rows.len();
            let mut hits = Vec::new();
            for i in candidates {
                let row = &rows[i];
                self.accelerator
                    .distances
                    .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                let d = score(q, &row[&field.field_name], &index.metric_type);
                if !d.is_finite() {
                    return Err(invalid("vector magnitude overflows finite distance"));
                }
                let asc = ascending(&index.metric_type);
                if radius.is_some_and(|v| if asc { d >= v } else { d <= v })
                    || range.is_some_and(|v| if asc { d < v } else { d > v })
                {
                    continue;
                }
                hits.push((i, d));
            }
            hits.sort_by(|a, b| {
                let cmp = if ascending(&index.metric_type) {
                    a.1.total_cmp(&b.1)
                } else {
                    b.1.total_cmp(&a.1)
                };
                cmp.then_with(|| a.0.cmp(&b.0))
            });
            let mut groups = BTreeSet::new();
            if let Some(g) = group {
                hits.retain(|(i, _)| groups.insert(rows[*i][g].to_string()));
            }
            for (i, d) in hits.into_iter().skip(offset).take(limit) {
                let row = &rows[i];
                let mut entity = project(row, &fields, &c.schema);
                entity["id"] = row[&c.schema.primary().field_name].clone();
                entity["distance"] = json!(d);
                if data.len() > 1 {
                    entity["queryIndex"] = json!(qi);
                }
                out.push(entity);
            }
        }
        Ok(json!(out))
    }
}
fn optional_f64(v: &Value, k: &str) -> Result<Option<f64>> {
    match v.get(k) {
        None => Ok(None),
        Some(x) => {
            Ok(Some(x.as_f64().filter(|n| n.is_finite()).ok_or_else(
                || invalid(format!("{k} must be finite number")),
            )?))
        }
    }
}
fn text<'a>(r: &'a Value, k: &str) -> Result<&'a str> {
    r[k].as_str()
        .filter(|s| !s.is_empty())
        .ok_or_else(|| invalid(format!("{k} must be a nonempty string")))
}
fn strings(r: &Value, k: &str) -> Result<Vec<String>> {
    match r.get(k) {
        None => Ok(vec![]),
        Some(v) => v
            .as_array()
            .ok_or_else(|| invalid(format!("{k} must be array")))?
            .iter()
            .map(|v| {
                v.as_str()
                    .map(str::to_owned)
                    .ok_or_else(|| invalid(format!("{k} elements must be strings")))
            })
            .collect(),
    }
}
pub(crate) fn pagination(r: &Value) -> Result<(usize, usize)> {
    let limit = number(&r["limit"], 10, 16384, "limit")?;
    let offset = number(&r["offset"], 0, 1000000, "offset")?;
    if limit == 0 {
        return Err(invalid("limit must be positive"));
    }
    Ok((limit, offset))
}
pub(crate) fn output_fields(c: &CollectionSpec, r: &Value) -> Result<Vec<String>> {
    let fields = strings(r, "outputFields")?;
    if fields.contains(&"count(*)".into()) && fields.len() != 1 {
        return Err(invalid("count(*) must be the only output field"));
    }
    for f in &fields {
        if f != "*" && f != "count(*)" && !c.schema.enable_dynamic_field {
            c.schema.field(f.split('.').next().unwrap())?;
        }
    }
    Ok(fields)
}
pub(crate) fn project(row: &Value, fields: &[String], s: &Schema) -> Value {
    if fields.iter().any(|f| f == "*") {
        return row.clone();
    }
    let mut out = json!({});
    let pk = &s.primary().field_name;
    out[pk] = row[pk].clone();
    for f in fields {
        out[f] = lookup(row, f).clone();
    }
    out
}
fn keys(v: &Value, allowed: &[&str], context: &str) -> Result<()> {
    let o = v
        .as_object()
        .ok_or_else(|| invalid(format!("{context} must be an object")))?;
    for k in o.keys() {
        if !allowed.contains(&k.as_str()) {
            return Err(Error::Unsupported(format!("{context} option {k}")));
        }
    }
    Ok(())
}

pub(crate) fn normalize_vectors(c: &CollectionSpec, row: &mut Value) -> Result<()> {
    for field in c
        .schema
        .fields
        .iter()
        .filter(|f| f.data_type == "FloatVector")
    {
        for element in row[&field.field_name]
            .as_array_mut()
            .ok_or_else(|| invalid("invalid dense vector"))?
        {
            let x = element
                .as_f64()
                .ok_or_else(|| invalid("invalid vector element"))?;
            let value = match c.dtype {
                VectorDtype::Float64 => x,
                VectorDtype::Float32 => f64::from(x as f32),
            };
            if !value.is_finite() {
                return Err(invalid("dense vector element overflow"));
            }
            *element = json!(value);
        }
    }
    Ok(())
}
