//! Embedded, transactional vector database. The HTTP server uses the same public API.
mod engine;
mod error;
mod filter;
mod index;
mod schema;
pub use engine::Engine;
pub use error::{Error, Result};
