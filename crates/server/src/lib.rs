use axum::{
    Json, Router,
    extract::{DefaultBodyLimit, Path, State, rejection::JsonRejection},
    http::{HeaderMap, StatusCode},
    response::{IntoResponse, Response},
    routing::{get, post},
};
use serde_json::{Value, json};
use std::sync::{
    Arc, Mutex,
    atomic::{AtomicU64, Ordering},
};
use tokio::sync::Semaphore;
use vectordb_engine::{Engine, Error};
#[derive(Clone)]
pub struct AppState {
    engine: Arc<Mutex<Engine>>,
    token: Option<Arc<str>>,
    slots: Arc<Semaphore>,
    requests: Arc<AtomicU64>,
    errors: Arc<AtomicU64>,
}
impl AppState {
    pub fn new(engine: Engine, token: Option<String>) -> Self {
        Self {
            engine: Arc::new(Mutex::new(engine)),
            token: token.map(Arc::from),
            slots: Arc::new(Semaphore::new(32)),
            requests: Arc::new(AtomicU64::new(0)),
            errors: Arc::new(AtomicU64::new(0)),
        }
    }
}
pub fn router(state: AppState) -> Router {
    Router::new()
        .route(
            "/healthz",
            get(|| async { Json(json!({"status":"ok","version":env!("CARGO_PKG_VERSION")})) }),
        )
        .route("/metrics", get(metrics))
        .route("/v2/vectordb/{category}/{action}", post(execute))
        .fallback(|| async { reply(StatusCode::NOT_FOUND, 100, "route not found") })
        .layer(DefaultBodyLimit::max(16 * 1024 * 1024))
        .with_state(state)
}
fn authorized(state: &AppState, headers: &HeaderMap) -> bool {
    let Some(expected) = &state.token else {
        return true;
    };
    if expected.is_empty() {
        return false;
    }
    let supplied = headers
        .get("authorization")
        .and_then(|h| h.to_str().ok())
        .and_then(|s| s.strip_prefix("Bearer "))
        .unwrap_or("");
    // No early exit based on matching token prefix.
    let mut diff = expected.len() ^ supplied.len();
    for (i, b) in expected.bytes().enumerate() {
        diff |= (b ^ supplied.as_bytes().get(i).copied().unwrap_or(0)) as usize;
    }
    diff == 0
}
fn reply(status: StatusCode, code: u16, message: &str) -> Response {
    (status, Json(json!({"code":code,"message":message}))).into_response()
}
async fn metrics(State(s): State<AppState>, h: HeaderMap) -> Response {
    if !authorized(&s, &h) {
        return reply(StatusCode::UNAUTHORIZED, 1800, "invalid bearer token");
    }
    ([("content-type","text/plain; version=0.0.4")],format!("# TYPE vectordb_requests_total counter\nvectordb_requests_total {}\n# TYPE vectordb_errors_total counter\nvectordb_errors_total {}\n",s.requests.load(Ordering::Relaxed),s.errors.load(Ordering::Relaxed))).into_response()
}
async fn execute(
    State(s): State<AppState>,
    Path((category, action)): Path<(String, String)>,
    headers: HeaderMap,
    body: Result<Json<Value>, JsonRejection>,
) -> Response {
    s.requests.fetch_add(1, Ordering::Relaxed);
    if !authorized(&s, &headers) {
        s.errors.fetch_add(1, Ordering::Relaxed);
        return reply(StatusCode::UNAUTHORIZED, 1800, "invalid bearer token");
    }
    let Json(request) = match body {
        Ok(v) => v,
        Err(e) => {
            s.errors.fetch_add(1, Ordering::Relaxed);
            return reply(e.status(), 1100, &e.body_text());
        }
    };
    let permit = match s.slots.clone().try_acquire_owned() {
        Ok(p) => p,
        Err(_) => {
            s.errors.fetch_add(1, Ordering::Relaxed);
            return reply(
                StatusCode::SERVICE_UNAVAILABLE,
                65535,
                "request queue full; retry later",
            );
        }
    };
    let operation = format!("{category}/{action}");
    let engine = s.engine.clone();
    let result = tokio::task::spawn_blocking(move || {
        let _permit = permit;
        let mut engine = engine
            .lock()
            .map_err(|_| Error::Conflict("engine unavailable after panic".into()))?;
        engine.execute(&operation, request)
    })
    .await;
    match result {
        Ok(Ok(data)) => Json(json!({"code":0,"data":data})).into_response(),
        Ok(Err(e)) => {
            s.errors.fetch_add(1, Ordering::Relaxed);
            let status = match &e {
                Error::Invalid(_) => StatusCode::BAD_REQUEST,
                Error::NotFound(_) => StatusCode::NOT_FOUND,
                Error::Conflict(_) => StatusCode::CONFLICT,
                Error::Unsupported(_) => StatusCode::NOT_IMPLEMENTED,
                _ => StatusCode::INTERNAL_SERVER_ERROR,
            };
            if status == StatusCode::INTERNAL_SERVER_ERROR {
                tracing::error!(error=%e,"database operation failed");
                reply(
                    status,
                    e.code(),
                    "internal storage error; inspect server log",
                )
            } else {
                reply(status, e.code(), &e.to_string())
            }
        }
        Err(e) => {
            s.errors.fetch_add(1, Ordering::Relaxed);
            tracing::error!(error=%e,"database worker failed");
            reply(
                StatusCode::INTERNAL_SERVER_ERROR,
                65535,
                "internal worker error",
            )
        }
    }
}
