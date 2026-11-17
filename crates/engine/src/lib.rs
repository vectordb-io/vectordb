//! Embedded, transactional vector database. The HTTP server uses the same public API.
mod engine;
mod error;
mod filter;
mod index;
mod schema;
pub use engine::Engine;
pub use error::{Error, Result};

pub mod segment;
pub use index::Index;
pub use schema::{Field, Schema};
pub mod segment_storage;

pub mod query_index;
