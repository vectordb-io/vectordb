//! Decode a final P1 command-history checkpoint using its original SQLite semantics.
use clap::Parser;
use serde::Deserialize;
use serde_json::Value;
use std::{collections::BTreeSet, fs, path::PathBuf};
use vectordb_cluster::{
    catalog::Manifest,
    digest,
    state::{Scope, request_digest},
};
use vectordb_engine::Engine;

#[derive(Parser)]
struct Args {
    #[arg(long)]
    manifest: PathBuf,
    #[arg(long)]
    checkpoint: PathBuf,
    #[arg(long)]
    output: PathBuf,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct History {
    format: u32,
    scope: Scope,
    offset: i64,
    records: Vec<Saved>,
}
#[derive(Deserialize)]
struct Saved {
    offset: i64,
    record: Record,
}
#[derive(Deserialize)]
struct Record {
    format: u32,
    scope: Scope,
    entry: Entry,
}
#[derive(Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case", deny_unknown_fields)]
enum Entry {
    Activate {
        epoch: i64,
    },
    Barrier {
        epoch: i64,
    },
    Mutation {
        epoch: i64,
        request_id: String,
        operation: String,
        request: Value,
        result: Value,
        digest: String,
    },
}
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();
    let manifest: Manifest = serde_json::from_slice(&fs::read(args.manifest)?)?;
    let bytes = fs::read(args.checkpoint)?;
    if bytes.len() != manifest.bytes || digest(&bytes) != manifest.sha256 {
        return Err("P1 checkpoint digest mismatch".into());
    }
    let history: History = serde_json::from_slice(&bytes)?;
    if history.format != 1 || history.scope != manifest.scope || history.offset != manifest.offset {
        return Err("invalid P1 checkpoint".into());
    }
    history.scope.validate()?;
    let mut engine = Engine::memory()?;
    let (mut epoch, mut offset) = (0, -1);
    let mut seen = BTreeSet::new();
    for saved in history.records {
        let record = saved.record;
        if record.format != 1 || record.scope != history.scope || saved.offset <= offset {
            return Err("foreign or unordered P1 record".into());
        }
        offset = saved.offset;
        match record.entry {
            Entry::Activate { epoch: next } if next > epoch => epoch = next,
            Entry::Barrier { epoch: e } if e == epoch && e > 0 => (),
            Entry::Mutation {
                epoch: e,
                request_id,
                operation,
                request,
                result,
                digest,
            } if e == epoch && e > 0 => {
                if !seen.insert(request_id)
                    || request_digest(&operation, &request)? != digest
                    || request["collectionName"] != history.scope.collection
                    || !matches!(
                        operation.as_str(),
                        "collections/create"
                            | "entities/insert"
                            | "entities/upsert"
                            | "indexes/create"
                            | "indexes/drop"
                            | "partitions/create"
                    )
                    || engine.execute(&operation, request)? != result
                {
                    return Err("P1 mutation failed replay verification".into());
                }
            }
            _ => return Err("invalid P1 epoch".into()),
        }
    }
    if offset != history.offset {
        return Err("incomplete P1 checkpoint".into());
    }
    let mutations = seen.len();
    engine.retain_legacy_keys(
        &history.scope.collection,
        seen.into_iter().filter(|id| id.starts_with("v2-")),
    )?;
    engine.backup(args.output)?;
    println!(
        "Verified P1 through offset {offset}, replayed {} mutation keys",
        mutations
    );
    Ok(())
}
