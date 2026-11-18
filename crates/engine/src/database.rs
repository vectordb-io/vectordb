//! Select the local storage format without modifying an existing SQLite file.
use crate::{Engine, LocalStore, Result};
use serde_json::Value;
use std::path::Path;

pub enum Database {
    SQLite(Engine),
    Segments(LocalStore),
}
impl From<Engine> for Database {
    fn from(value: Engine) -> Self {
        Self::SQLite(value)
    }
}
impl Database {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let path = path.as_ref();
        if path == Path::new(":memory:") {
            return Ok(Self::SQLite(Engine::memory()?));
        }
        if path.is_dir()
            || (!path.exists()
                && !matches!(
                    path.extension().and_then(|e| e.to_str()),
                    Some("sqlite" | "db" | "sqlite3")
                ))
        {
            Ok(Self::Segments(LocalStore::open(path)?))
        } else {
            Ok(Self::SQLite(Engine::open(path)?))
        }
    }
    pub fn execute(&mut self, op: &str, req: Value) -> Result<Value> {
        match self {
            Self::SQLite(e) => e.execute(op, req),
            Self::Segments(e) => e.execute(op, req),
        }
    }
    pub fn flush(&mut self) -> Result<()> {
        match self {
            Self::SQLite(e) => e.flush(),
            Self::Segments(e) => e.flush(),
        }
    }
    pub fn backup(&self, path: impl AsRef<Path>) -> Result<()> {
        match self {
            Self::SQLite(e) => e.backup(path),
            Self::Segments(e) => e.backup(path),
        }
    }
    pub fn export_jsonl(
        &self,
        db: &str,
        collection: &str,
        writer: impl std::io::Write,
    ) -> Result<usize> {
        match self {
            Self::SQLite(e) => e.export_jsonl(db, collection, writer),
            Self::Segments(e) => e.export_jsonl(db, collection, writer),
        }
    }
    pub fn import_jsonl(
        &mut self,
        db: &str,
        collection: &str,
        reader: impl std::io::BufRead,
        upsert: bool,
        batch: usize,
    ) -> Result<usize> {
        match self {
            Self::SQLite(e) => e.import_jsonl(db, collection, reader, upsert, batch),
            Self::Segments(e) => e.import_jsonl(db, collection, reader, upsert, batch),
        }
    }
}
