use thiserror::Error;
pub type Result<T> = std::result::Result<T, Error>;
#[derive(Debug, Error)]
pub enum Error {
    #[error("{0}")]
    Invalid(String),
    #[error("{0}")]
    NotFound(String),
    #[error("{0}")]
    Conflict(String),
    #[error("unsupported: {0}")]
    Unsupported(String),
    #[error("storage: {0}")]
    Storage(#[from] rusqlite::Error),
    #[error("JSON: {0}")]
    Json(#[from] serde_json::Error),
    #[error("I/O: {0}")]
    Io(#[from] std::io::Error),
}
impl Error {
    pub fn code(&self) -> u16 {
        match self {
            Self::Invalid(_) => 1100,
            Self::NotFound(_) => 100,
            Self::Conflict(_) => 1101,
            Self::Unsupported(_) => 1102,
            _ => 65535,
        }
    }
}
pub(crate) fn invalid(message: impl Into<String>) -> Error {
    Error::Invalid(message.into())
}
