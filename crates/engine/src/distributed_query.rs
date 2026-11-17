//! Bounded global merge. Hybrid scores are computed only after each subsearch's
//! shard candidates have been merged into a global ranking at the same snapshot.
use crate::{
    Error, Result,
    error::invalid,
    filter::compare,
    index::ascending,
    segment::{CollectionSpec, SegmentEngine, output_fields, pagination, project},
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::BTreeMap;
pub const MAX_WINDOW: usize = 16384;
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct QueryPlan {
    pub operation: String,
    pub requests: Vec<Value>,
    original: Value,
    spec: CollectionSpec,
    subrequests: Vec<Value>,
}
fn metric<'a>(spec: &'a CollectionSpec, r: &Value) -> Result<&'a str> {
    let field = r["annsField"]
        .as_str()
        .or_else(|| {
            let fields = spec
                .schema
                .fields
                .iter()
                .filter(|f| f.is_vector())
                .collect::<Vec<_>>();
            (fields.len() == 1).then(|| fields[0].field_name.as_str())
        })
        .ok_or_else(|| invalid("annsField required"))?;
    spec.indexes
        .iter()
        .find(|i| i.field_name == field)
        .map(|i| i.metric_type.as_str())
        .ok_or_else(|| invalid("vector field has no index"))
}
fn window(r: &Value) -> Result<(usize, usize)> {
    let (l, o) = pagination(r)?;
    if l.saturating_add(o) > MAX_WINDOW {
        return Err(invalid("distributed limit + offset exceeds 16384"));
    }
    Ok((l, o))
}
fn wire_request(spec: &CollectionSpec, op: &str, r: &Value) -> Result<Value> {
    if r.get("groupingField").is_some() {
        return Err(Error::Unsupported(
            "distributed grouping is not enabled".into(),
        ));
    }
    let (l, o) = window(r)?;
    let mut wire = r.clone();
    if op != "entities/count" {
        wire["limit"] = json!(l + o);
        wire["offset"] = json!(0);
    }
    let mut fields = output_fields(spec, r)?;
    if let Some(order) = r["orderByFields"].as_array() {
        for f in order {
            fields.push(
                f.as_str()
                    .ok_or_else(|| invalid("invalid order field"))?
                    .split(':')
                    .next()
                    .unwrap()
                    .into(),
            );
        }
    }
    if !fields.is_empty() {
        wire["outputFields"] = json!(fields);
    }
    // The empty engine validates schema, search params, vectors and expressions
    // without materializing any collection data in the gateway.
    let mut empty = SegmentEngine::new();
    empty.spec = Some(spec.clone());
    empty.execute(op, wire.clone())?;
    Ok(wire)
}
impl QueryPlan {
    pub fn new(spec: CollectionSpec, op: &str, r: Value) -> Result<Self> {
        crate::validate_request(op, &r)?;
        output_fields(&spec, &r)?;
        window(&r)?;
        let mut subrequests = vec![];
        if op == "entities/hybrid_search" {
            let searches = r["search"]
                .as_array()
                .filter(|v| !v.is_empty() && v.len() <= 16)
                .ok_or_else(|| invalid("hybrid requires 1..16 subsearches"))?;
            let strategy = r["rerank"]["strategy"].as_str().unwrap_or("");
            if !["rrf", "weighted"].contains(&strategy) {
                return Err(invalid("reranker must be rrf or weighted"));
            }
            let rerank = r["rerank"]
                .as_object()
                .ok_or_else(|| invalid("rerank required"))?;
            if rerank
                .keys()
                .any(|k| !["strategy", "params"].contains(&k.as_str()))
            {
                return Err(invalid("unknown rerank option"));
            }
            if let Some(params) = r["rerank"].get("params") {
                let params = params
                    .as_object()
                    .ok_or_else(|| invalid("rerank params must be object"))?;
                if params
                    .keys()
                    .any(|k| k != if strategy == "rrf" { "k" } else { "weights" })
                {
                    return Err(invalid("unknown rerank parameter"));
                }
            }
            if strategy == "rrf" {
                if r["rerank"]["params"]
                    .get("k")
                    .is_some_and(|k| k.as_f64().is_none_or(|v| !v.is_finite() || v <= 0.))
                {
                    return Err(invalid("RRF k must be positive"));
                }
            } else {
                let ws = r["rerank"]["params"]["weights"]
                    .as_array()
                    .filter(|w| w.len() == searches.len())
                    .ok_or_else(|| invalid("one weight per subsearch required"))?;
                if ws
                    .iter()
                    .any(|w| w.as_f64().is_none_or(|w| !(0.0..=1.0).contains(&w)))
                    || ws.iter().all(|w| w.as_f64() == Some(0.))
                {
                    return Err(invalid("weights must be in [0,1] and not all zero"));
                }
            }
            let nq = searches[0]["data"].as_array().map(Vec::len);
            for search in searches {
                let keys = search
                    .as_object()
                    .ok_or_else(|| invalid("subsearch must be object"))?;
                if keys.keys().any(|k| {
                    ![
                        "data",
                        "annsField",
                        "filter",
                        "limit",
                        "offset",
                        "metricType",
                        "params",
                    ]
                    .contains(&k.as_str())
                }) {
                    return Err(invalid("unknown subsearch option"));
                }
                if search["data"].as_array().map(Vec::len) != nq {
                    return Err(invalid("subsearch query counts differ"));
                }
                let mut child = search.clone();
                child.as_object_mut().unwrap().remove("metricType");
                child.as_object_mut().unwrap().remove("params");
                child["collectionName"] = r["collectionName"].clone();
                if let Some(p) = r.get("partitionNames") {
                    child["partitionNames"] = p.clone();
                }
                child["outputFields"] = r.get("outputFields").cloned().unwrap_or(json!([]));
                let mut params = json!({});
                if let Some(m) = search.get("metricType") {
                    params["metricType"] = m.clone();
                }
                if let Some(p) = search.get("params") {
                    params["params"] = p.clone();
                }
                child["searchParams"] = params;
                subrequests.push(child);
            }
        } else {
            subrequests.push(r.clone());
        }
        let operation = if op == "entities/hybrid_search" {
            "entities/search"
        } else {
            op
        }
        .to_string();
        let requests = subrequests
            .iter()
            .map(|r| wire_request(&spec, &operation, r))
            .collect::<Result<Vec<_>>>()?;
        Ok(Self {
            operation,
            requests,
            original: r,
            spec,
            subrequests,
        })
    }
    pub fn merge(&self, parts: Vec<Vec<Value>>) -> Result<Value> {
        if parts.is_empty() || parts.iter().any(|p| p.len() != self.requests.len()) {
            return Err(Error::Integrity("incomplete subsearch response".into()));
        }
        let mut ranked = vec![];
        for (n, r) in self.subrequests.iter().enumerate() {
            ranked.push(self.merge_one(r, parts.iter().map(|p| p[n].clone()).collect())?);
        }
        if self.original.get("search").is_none() {
            return Ok(ranked.remove(0));
        }
        let strategy = self.original["rerank"]["strategy"].as_str().unwrap();
        let k = self.original["rerank"]["params"]["k"]
            .as_f64()
            .unwrap_or(60.);
        let mut combined: BTreeMap<(u64, String), (Value, f64)> = BTreeMap::new();
        for (n, hits) in ranked.iter().enumerate() {
            let mut ranks = BTreeMap::<u64, usize>::new();
            let metric = metric(&self.spec, &self.subrequests[n])?;
            for hit in hits.as_array().unwrap() {
                let qi = hit["queryIndex"].as_u64().unwrap_or(0);
                let rank = ranks.entry(qi).or_default();
                *rank += 1;
                let d = hit["distance"]
                    .as_f64()
                    .ok_or_else(|| invalid("invalid distance"))?;
                let contribution = if strategy == "rrf" {
                    1. / (k + *rank as f64)
                } else {
                    let norm = match metric {
                        "COSINE" => (1. + d) / 2.,
                        "IP" => 0.5 + d.atan() / std::f64::consts::PI,
                        _ => 1. - 2. * d.atan() / std::f64::consts::PI,
                    };
                    self.original["rerank"]["params"]["weights"][n]
                        .as_f64()
                        .unwrap()
                        * norm
                };
                let entry = combined
                    .entry((qi, hit["id"].to_string()))
                    .or_insert((hit.clone(), 0.));
                entry.1 += contribution;
            }
        }
        let nq = self.subrequests[0]["data"].as_array().unwrap().len();
        let (l, o) = window(&self.original)?;
        let mut out = vec![];
        for qi in 0..nq {
            let mut hits = combined
                .iter()
                .filter(|((q, _), _)| *q == qi as u64)
                .collect::<Vec<_>>();
            hits.sort_by(|a, b| b.1.1.total_cmp(&a.1.1).then_with(|| a.0.1.cmp(&b.0.1)));
            for (_, (hit, score)) in hits.into_iter().skip(o).take(l) {
                let mut v = hit.clone();
                v["distance"] = json!(score);
                out.push(v);
            }
        }
        Ok(json!(out))
    }
    fn merge_one(&self, r: &Value, parts: Vec<Value>) -> Result<Value> {
        if self.operation == "entities/count" || r["outputFields"] == json!(["count(*)"]) {
            let mut count = 0u64;
            for p in parts {
                let value = if self.operation == "entities/count" {
                    &p["count"]
                } else {
                    &p[0]["count(*)"]
                };
                count = count
                    .checked_add(
                        value
                            .as_u64()
                            .ok_or_else(|| invalid("invalid shard count response"))?,
                    )
                    .ok_or_else(|| invalid("count overflow"))?;
            }
            return Ok(if self.operation == "entities/count" {
                json!({"count":count})
            } else {
                json!([{"count(*)":count}])
            });
        }
        let mut rows = vec![];
        for p in parts {
            rows.extend(
                p.as_array()
                    .ok_or_else(|| invalid("invalid shard rows"))?
                    .iter()
                    .cloned(),
            );
        }
        let pk = &self.spec.schema.primary().field_name;
        let search = self.operation == "entities/search";
        let asc = if search {
            ascending(metric(&self.spec, r)?)
        } else {
            true
        };
        if search
            && rows
                .iter()
                .any(|v| v["distance"].as_f64().is_none_or(|v| !v.is_finite()))
        {
            return Err(invalid("invalid shard score"));
        }
        rows.sort_by(|a, b| {
            if search {
                let cmp = a["queryIndex"]
                    .as_u64()
                    .unwrap_or(0)
                    .cmp(&b["queryIndex"].as_u64().unwrap_or(0));
                if !cmp.is_eq() {
                    return cmp;
                }
                let cmp = a["distance"]
                    .as_f64()
                    .unwrap()
                    .total_cmp(&b["distance"].as_f64().unwrap());
                if !cmp.is_eq() {
                    return if asc { cmp } else { cmp.reverse() };
                }
            } else if let Some(order) = r["orderByFields"].as_array() {
                for field in order {
                    let field = field.as_str().unwrap();
                    let name = field.split(':').next().unwrap();
                    let cmp = compare(&a[name], &b[name])
                        .unwrap_or_else(|| a[name].is_null().cmp(&b[name].is_null()));
                    let cmp = if field.ends_with(":desc") {
                        cmp.reverse()
                    } else {
                        cmp
                    };
                    if !cmp.is_eq() {
                        return cmp;
                    }
                }
            }
            a[if search { "id" } else { pk }]
                .to_string()
                .cmp(&b[if search { "id" } else { pk }].to_string())
        });
        let (l, o) = window(r)?;
        let fields = output_fields(&self.spec, r)?;
        let nq = if search {
            r["data"].as_array().unwrap().len()
        } else {
            1
        };
        let mut out = vec![];
        for qi in 0..nq {
            let mut seen = std::collections::BTreeSet::new();
            for row in rows
                .iter()
                .filter(|row| !search || row["queryIndex"].as_u64().unwrap_or(0) == qi as u64)
            {
                if !seen.insert(row[if search { "id" } else { pk }].to_string()) {
                    return Err(Error::Integrity("duplicate PK across shards".into()));
                }
            }
            for row in rows
                .iter()
                .filter(|row| !search || row["queryIndex"].as_u64().unwrap_or(0) == qi as u64)
                .skip(o)
                .take(l)
            {
                let mut v = project(row, &fields, &self.spec.schema);
                if search {
                    v["id"] = row["id"].clone();
                    v["distance"] = row["distance"].clone();
                    if nq > 1 {
                        v["queryIndex"] = json!(qi);
                    }
                }
                out.push(v);
            }
        }
        Ok(json!(out))
    }
}
