//! State format 2: immutable Parquet scalar columns and PK/version columns, plus
//! little-endian vector buffers. No command history is required to load a snapshot.
use crate::{
    Error, Result, Schema,
    segment::{
        CollectionSpec, MAX_METADATA_BYTES, MAX_STATE_BYTES, STATE_FORMAT, SegmentEngine,
        VectorDtype, VersionedRow,
    },
};
use arrow_array::{Array, ArrayRef, RecordBatch, StringArray};
use arrow_schema::{DataType, Field, Schema as ArrowSchema};
use parquet::arrow::{ArrowWriter, arrow_reader::ParquetRecordBatchReaderBuilder};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use sha2::{Digest, Sha256};
use std::{
    collections::{BTreeMap, BTreeSet},
    io::{Cursor, Read},
    sync::Arc,
};

pub fn checksum(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}
fn storage(e: impl std::fmt::Display) -> Error {
    Error::Integrity(e.to_string())
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ObjectRef {
    pub key: String,
    pub bytes: usize,
    pub sha256: String,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct SegmentManifest {
    pub format: u32,
    pub ordinal: usize,
    #[serde(default)]
    pub shard: Option<usize>,
    pub rows: usize,
    pub scalars: ObjectRef,
    pub versions: ObjectRef,
    pub vectors: ObjectRef,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct StateManifest {
    pub format: u32,
    pub revision: i64,
    pub spec: Option<CollectionSpec>,
    pub segments: Vec<SegmentManifest>,
    #[serde(default)]
    pub indexes: Vec<ObjectRef>,
}
/// Sealing owns an immutable snapshot. Persist all objects before publishing the
/// manifest; callers retain this object until publication succeeds or fails.
pub struct SealedState {
    pub manifest: StateManifest,
    pub objects: Vec<(String, Vec<u8>)>,
}
fn parquet(columns: Vec<(String, Vec<Option<String>>)>) -> Result<Vec<u8>> {
    let schema = Arc::new(ArrowSchema::new(
        columns
            .iter()
            .map(|(n, _)| Field::new(n, DataType::Utf8, true))
            .collect::<Vec<_>>(),
    ));
    let arrays = columns
        .into_iter()
        .map(|(_, v)| Arc::new(StringArray::from(v)) as ArrayRef)
        .collect();
    let batch = RecordBatch::try_new(schema.clone(), arrays).map_err(storage)?;
    let mut bytes = vec![];
    let mut writer = ArrowWriter::try_new(&mut bytes, schema, None).map_err(storage)?;
    writer.write(&batch).map_err(storage)?;
    writer.close().map_err(storage)?;
    Ok(bytes)
}
fn read_parquet(bytes: Vec<u8>, expected: usize) -> Result<BTreeMap<String, Vec<Option<String>>>> {
    let builder =
        ParquetRecordBatchReaderBuilder::try_new(bytes::Bytes::from(bytes)).map_err(storage)?;
    if builder.metadata().file_metadata().num_rows() != expected as i64 {
        return Err(storage("row count mismatch"));
    }
    let mut out: BTreeMap<String, Vec<Option<String>>> = BTreeMap::new();
    for batch in builder.with_batch_size(4096).build().map_err(storage)? {
        let batch = batch.map_err(storage)?;
        for (i, field) in batch.schema().fields().iter().enumerate() {
            let col = batch
                .column(i)
                .as_any()
                .downcast_ref::<StringArray>()
                .ok_or_else(|| storage("scalar column type"))?;
            let dest = out.entry(field.name().clone()).or_default();
            dest.extend((0..col.len()).map(|i| {
                if col.is_null(i) {
                    None
                } else {
                    Some(col.value(i).into())
                }
            }));
        }
    }
    Ok(out)
}
fn append_len(bytes: &mut Vec<u8>, value: &[u8]) -> Result<()> {
    let n = u32::try_from(value.len()).map_err(storage)?;
    bytes.extend(n.to_le_bytes());
    bytes.extend(value);
    Ok(())
}
fn take(cur: &mut Cursor<&[u8]>) -> Result<Vec<u8>> {
    let mut n = [0; 4];
    cur.read_exact(&mut n)?;
    let n = u32::from_le_bytes(n) as usize;
    if n > cur.get_ref().len().saturating_sub(cur.position() as usize) {
        return Err(storage("truncated vector buffer"));
    }
    let mut out = vec![0; n];
    cur.read_exact(&mut out)?;
    Ok(out)
}
impl SegmentEngine {
    pub fn seal(&self) -> Result<SealedState> {
        if self.selection.is_some() {
            return Err(storage(
                "partial query state cannot publish a writer checkpoint",
            ));
        }
        let count = self.spec.as_ref().map_or(1, |c| c.shard_count);
        let mut sealed = SealedState {
            manifest: StateManifest {
                format: if count > 1 { 4 } else { 3 },
                revision: self.revision,
                spec: self.spec.clone(),
                segments: vec![],
                indexes: vec![],
            },
            objects: vec![],
        };
        let Some(spec) = &self.spec else {
            return Ok(sealed);
        };
        let mut rows = self.rows.values().collect::<Vec<_>>();
        rows.sort_by_key(|r| crate::shard::shard_for(&r.key, count));
        // Bound individual objects; current-state admission bounds the whole seal.
        let mut start = 0;
        while start < rows.len() {
            let mut end = start;
            let mut size = 0;
            while end < rows.len()
                && end - start < 4096
                && crate::shard::shard_for(&rows[end].key, count)
                    == crate::shard::shard_for(&rows[start].key, count)
            {
                let n = serde_json::to_vec(rows[end])?.len();
                if end > start && size + n > 4 * 1024 * 1024 {
                    break;
                }
                size += n;
                end += 1;
            }
            let chunk = &rows[start..end];
            let ordinal = sealed.manifest.segments.len();
            let vector_fields = spec
                .schema
                .fields
                .iter()
                .filter(|f| f.is_vector())
                .collect::<Vec<_>>();
            let mut names = BTreeSet::new();
            for r in chunk {
                if let Some(obj) = r.body.as_ref().and_then(Value::as_object) {
                    names.extend(
                        obj.keys()
                            .filter(|n| !vector_fields.iter().any(|f| &f.field_name == *n))
                            .cloned(),
                    );
                }
            }
            // A reserved physical marker guarantees a nonempty scalar schema for tombstone-only segments.
            let mut cols = vec![(
                "$row".into(),
                chunk.iter().map(|r| Some(r.key.clone())).collect(),
            )];
            for n in names {
                cols.push((
                    format!("field:{n}"),
                    chunk
                        .iter()
                        .map(|r| {
                            r.body
                                .as_ref()
                                .and_then(|v| v.get(&n))
                                .map(Value::to_string)
                        })
                        .collect(),
                ));
            }
            let scalars = parquet(cols)?;
            let versions = parquet(vec![
                (
                    "pk".into(),
                    chunk.iter().map(|r| Some(r.key.clone())).collect(),
                ),
                (
                    "partition".into(),
                    chunk.iter().map(|r| Some(r.partition.clone())).collect(),
                ),
                (
                    "version".into(),
                    chunk.iter().map(|r| Some(r.version.to_string())).collect(),
                ),
                (
                    "tombstone".into(),
                    chunk
                        .iter()
                        .map(|r| Some(r.body.is_none().to_string()))
                        .collect(),
                ),
            ])?;
            let header = json!({"format":STATE_FORMAT,"dtype":spec.dtype,"rows":chunk.len(),"fields":vector_fields});
            let mut vectors = b"VDBVEC02".to_vec();
            append_len(&mut vectors, &serde_json::to_vec(&header)?)?;
            for r in chunk {
                for f in &vector_fields {
                    let mut data = vec![];
                    if let Some(row) = &r.body {
                        match f.data_type.as_str() {
                            "FloatVector" => {
                                for v in row[&f.field_name]
                                    .as_array()
                                    .ok_or_else(|| storage("missing vector"))?
                                {
                                    let v = v.as_f64().ok_or_else(|| storage("vector number"))?;
                                    match spec.dtype {
                                        VectorDtype::Float32 => {
                                            data.extend((v as f32).to_le_bytes())
                                        }
                                        VectorDtype::Float64 => data.extend(v.to_le_bytes()),
                                    }
                                }
                            }
                            "BinaryVector" => {
                                for v in row[&f.field_name]
                                    .as_array()
                                    .ok_or_else(|| storage("binary vector"))?
                                {
                                    data.push(
                                        v.as_u64().ok_or_else(|| storage("binary byte"))? as u8
                                    )
                                }
                            }
                            _ => data = serde_json::to_vec(&row[&f.field_name])?,
                        }
                    }
                    append_len(&mut vectors, &data)?;
                }
            }
            let mut object = |kind: &str, data: Vec<u8>| -> ObjectRef {
                let key = format!("segment-{ordinal}-{kind}");
                let reference = ObjectRef {
                    key: key.clone(),
                    bytes: data.len(),
                    sha256: checksum(&data),
                };
                sealed.objects.push((key, data));
                reference
            };
            sealed.manifest.segments.push(SegmentManifest {
                format: STATE_FORMAT,
                ordinal,
                shard: (count > 1).then(|| crate::shard::shard_for(&rows[start].key, count)),
                rows: chunk.len(),
                scalars: object("scalars.parquet", scalars),
                versions: object("versions.parquet", versions),
                vectors: object("vectors.bin", vectors),
            });
            start = end;
        }
        for (key, bytes) in self.persist_indexes()? {
            sealed.manifest.indexes.push(ObjectRef {
                key: key.clone(),
                bytes: bytes.len(),
                sha256: checksum(&bytes),
            });
            sealed.objects.push((key, bytes));
        }
        Ok(sealed)
    }
    pub fn load(manifest: StateManifest, get: impl FnMut(&str) -> Result<Vec<u8>>) -> Result<Self> {
        Self::load_selected(manifest, None, get)
    }
    pub fn load_shards(
        manifest: StateManifest,
        selected: BTreeSet<usize>,
        get: impl FnMut(&str) -> Result<Vec<u8>>,
    ) -> Result<Self> {
        Self::load_selected(manifest, Some(selected), get)
    }
    fn load_selected(
        manifest: StateManifest,
        selected: Option<BTreeSet<usize>>,
        mut get: impl FnMut(&str) -> Result<Vec<u8>>,
    ) -> Result<Self> {
        if ![STATE_FORMAT, 3, 4].contains(&manifest.format)
            || (manifest.format == STATE_FORMAT && !manifest.indexes.is_empty())
            || manifest.revision < 0
        {
            return Err(storage("unsupported state format or revision"));
        }
        let mut engine = Self::new();
        engine.revision = manifest.revision;
        engine.selection = selected.clone();
        let Some(spec) = manifest.spec else {
            if !manifest.segments.is_empty() || !manifest.indexes.is_empty() {
                return Err(storage("segments without schema"));
            }
            return Ok(engine);
        };
        if spec.format != STATE_FORMAT || spec.next_id < 1 || !spec.partitions.contains("_default")
        {
            return Err(storage("invalid collection metadata"));
        }
        if serde_json::to_vec(&spec)?.len() > MAX_METADATA_BYTES {
            return Err(storage("metadata exceeds quota"));
        }
        crate::shard::validate_count(spec.shard_count)?;
        if (manifest.format < 4 && spec.shard_count != 1)
            || selected
                .as_ref()
                .is_some_and(|s| s.is_empty() || s.iter().any(|id| *id >= spec.shard_count))
        {
            return Err(storage("invalid shard layout"));
        }
        Schema::from_request(&json!({"schema":spec.schema}))?;
        for i in &spec.indexes {
            let mut i = i.clone();
            i.validate(spec.schema.field(&i.field_name)?)?;
        }
        let mut total_bytes = 0usize;
        for (ordinal, segment) in manifest.segments.iter().enumerate() {
            if segment.format != STATE_FORMAT
                || segment.ordinal != ordinal
                || segment.rows == 0
                || segment.rows > 4096
            {
                return Err(storage("invalid segment descriptor"));
            }
            if manifest.format == 4 && segment.shard.is_none_or(|s| s >= spec.shard_count) {
                return Err(storage("missing/invalid shard descriptor"));
            }
            if manifest.format < 4 && segment.shard.is_some() {
                return Err(storage("unexpected shard descriptor"));
            }
            if selected
                .as_ref()
                .is_some_and(|s| !s.contains(&segment.shard.unwrap_or(0)))
            {
                continue;
            }
            let mut fetch = |r: &ObjectRef| -> Result<Vec<u8>> {
                if r.bytes > 64 * 1024 * 1024 {
                    return Err(storage("object exceeds quota"));
                }
                let bytes = get(&r.key)?;
                if bytes.len() != r.bytes || checksum(&bytes) != r.sha256 {
                    return Err(storage("checksum or length mismatch"));
                }
                total_bytes += bytes.len();
                if total_bytes > MAX_STATE_BYTES * 4 {
                    return Err(storage("snapshot exceeds quota"));
                }
                Ok(bytes)
            };
            let scalars = read_parquet(fetch(&segment.scalars)?, segment.rows)?;
            let versions = read_parquet(fetch(&segment.versions)?, segment.rows)?;
            let vectors = fetch(&segment.vectors)?;
            if !vectors.starts_with(b"VDBVEC02") {
                return Err(storage("bad vector magic"));
            }
            let mut cur = Cursor::new(&vectors[8..]);
            let header: Value = serde_json::from_slice(&take(&mut cur)?)?;
            let fields = spec
                .schema
                .fields
                .iter()
                .filter(|f| f.is_vector())
                .collect::<Vec<_>>();
            if header
                != json!({"format":STATE_FORMAT,"dtype":spec.dtype,"rows":segment.rows,"fields":fields})
            {
                return Err(storage("vector header mismatch"));
            }
            for n in 0..segment.rows {
                let value = |name: &str| -> Result<&str> {
                    versions
                        .get(name)
                        .and_then(|c| c.get(n))
                        .and_then(Option::as_deref)
                        .ok_or_else(|| storage("missing version column"))
                };
                let key = value("pk")?.to_string();
                if manifest.format == 4
                    && Some(crate::shard::shard_for(&key, spec.shard_count)) != segment.shard
                {
                    return Err(storage("row belongs to another shard"));
                }
                let partition = value("partition")?.to_string();
                let version: i64 = value("version")?.parse().map_err(storage)?;
                let tombstone: bool = value("tombstone")?.parse().map_err(storage)?;
                if version <= 0
                    || version > manifest.revision
                    || !spec.partitions.contains(&partition)
                {
                    return Err(storage("invalid row version or partition"));
                }
                if scalars
                    .get("$row")
                    .and_then(|c| c.get(n))
                    .and_then(Option::as_ref)
                    != Some(&key)
                {
                    return Err(storage("PK column alignment mismatch"));
                }
                let mut row = json!({});
                for (field, col) in &scalars {
                    if field != "$row"
                        && let Some(v) = col.get(n).and_then(Option::as_ref)
                    {
                        let field = field
                            .strip_prefix("field:")
                            .ok_or_else(|| storage("invalid scalar column"))?;
                        row[field] = serde_json::from_str(v)?;
                    }
                }
                for f in &fields {
                    let bytes = take(&mut cur)?;
                    if tombstone {
                        if !bytes.is_empty() {
                            return Err(storage("tombstone vector present"));
                        }
                        continue;
                    }
                    row[&f.field_name] = match f.data_type.as_str() {
                        "FloatVector" => {
                            let width = if spec.dtype == VectorDtype::Float32 {
                                4
                            } else {
                                8
                            };
                            if bytes.len() != f.dim()? * width {
                                return Err(storage("vector dimension mismatch"));
                            }
                            let values: Vec<f64> = bytes
                                .chunks_exact(width)
                                .map(|b| {
                                    if width == 4 {
                                        f64::from(f32::from_le_bytes(b.try_into().unwrap()))
                                    } else {
                                        f64::from_le_bytes(b.try_into().unwrap())
                                    }
                                })
                                .collect();
                            if values.iter().any(|v| !v.is_finite()) {
                                return Err(storage("non-finite vector"));
                            }
                            json!(values)
                        }
                        "BinaryVector" => json!(bytes),
                        _ => serde_json::from_slice(&bytes)?,
                    };
                }
                let pk: Value = serde_json::from_str(&key)?;
                spec.schema.primary().validate(&pk)?;
                let canonical = pk.to_string();
                if canonical != key {
                    return Err(storage("noncanonical primary key"));
                }
                if !tombstone {
                    spec.schema.validate_row(&mut row)?;
                    if row[&spec.schema.primary().field_name] != pk {
                        return Err(storage("primary key mismatch"));
                    }
                }
                if spec.schema.auto_id && pk.as_i64().is_some_and(|pk| pk >= spec.next_id) {
                    return Err(storage("autoID watermark behind data"));
                }
                let r = VersionedRow {
                    key: key.clone(),
                    partition,
                    version,
                    body: if tombstone { None } else { Some(row) },
                };
                if engine.rows.insert(key, Arc::new(r)).is_some() {
                    return Err(storage("duplicate snapshot PK"));
                }
            }
            if cur.position() as usize != vectors.len() - 8 {
                return Err(storage("trailing vector bytes"));
            }
        }
        engine.spec = Some(spec);
        let mut index_keys = BTreeSet::new();
        for reference in &manifest.indexes {
            if !index_keys.insert(&reference.key) {
                return Err(storage("duplicate index object"));
            }
            if let Some(selection) = &selected {
                let bucket = reference
                    .key
                    .strip_prefix("index-bucket-")
                    .and_then(|s| s.strip_suffix(".json"))
                    .and_then(|s| s.parse::<usize>().ok())
                    .filter(|b| *b < crate::query_index::BUCKETS)
                    .ok_or_else(|| storage("invalid index bucket reference"))?;
                if !selection.contains(&(bucket % engine.spec.as_ref().unwrap().shard_count)) {
                    continue;
                }
            }
            engine.load_index(reference, get(&reference.key)?)?;
        }
        engine.recount_bytes();
        if engine.bytes > MAX_STATE_BYTES {
            return Err(storage("state exceeds quota"));
        }
        Ok(engine)
    }
}
