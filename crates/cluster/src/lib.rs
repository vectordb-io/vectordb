//! Experimental multi-collection catalog, ordered logs and shared checkpoints.
//! M0-M5 recovery, sharding and coordinated compute; production qualification remains open.
pub mod catalog;
pub mod kafka;
pub mod object;
pub mod state;

use thiserror::Error;
pub type Result<T> = std::result::Result<T, Error>;
#[derive(Debug, Error)]
pub enum Error {
    #[error("WAL prefix reclaimed; reload checkpoint")]
    CheckpointRequired,
    #[error("invalid request: {0}")]
    Invalid(String),
    #[error("conflict: {0}")]
    Conflict(String),
    #[error("unsupported: {0}")]
    Unsupported(String),
    #[error("unavailable: {0}")]
    Unavailable(String),
    #[error("storage integrity failure: {0}")]
    Integrity(String),
    #[error(transparent)]
    Engine(#[from] vectordb_engine::Error),
    #[error(transparent)]
    Json(#[from] serde_json::Error),
    #[error(transparent)]
    Http(#[from] reqwest::Error),
    #[error(transparent)]
    Kafka(#[from] rdkafka::error::KafkaError),
    #[error(transparent)]
    Io(#[from] std::io::Error),
}
pub fn digest(bytes: &[u8]) -> String {
    use sha2::{Digest, Sha256};
    format!("{:x}", Sha256::digest(bytes))
}
pub fn identifier(value: &str) -> Result<()> {
    if value.is_empty()
        || value.len() > 64
        || !value
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'-' || b == b'_')
    {
        return Err(Error::Invalid(
            "identifier must contain 1..64 ASCII letters, digits, '-' or '_'".into(),
        ));
    }
    Ok(())
}

pub mod contracts;

pub mod directory;

pub mod sharded;

pub mod control;
pub mod transport;
