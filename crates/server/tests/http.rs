use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use http_body_util::BodyExt;
use serde_json::{Value, json};
use tower::ServiceExt;
use vectordb_engine::Engine;
use vectordb_server::{AppState, router};
async fn request(
    app: axum::Router,
    path: &str,
    body: &str,
    token: Option<&str>,
) -> (StatusCode, Value) {
    let mut r = Request::builder()
        .method("POST")
        .uri(path)
        .header("content-type", "application/json");
    if let Some(t) = token {
        r = r.header("authorization", format!("Bearer {t}"));
    }
    let response = app
        .oneshot(r.body(Body::from(body.to_owned())).unwrap())
        .await
        .unwrap();
    let status = response.status();
    let bytes = response.into_body().collect().await.unwrap().to_bytes();
    (status, serde_json::from_slice(&bytes).unwrap())
}
#[tokio::test]
async fn authentication_errors_and_real_requests() {
    let app = router(AppState::new(
        Engine::memory().unwrap(),
        Some("secret".into()),
    ));
    let path = "/v2/vectordb/collections/create";
    let body = json!({"collectionName":"items","dimension":2}).to_string();
    assert_eq!(
        request(app.clone(), path, &body, None).await.0,
        StatusCode::UNAUTHORIZED
    );
    assert_eq!(
        request(app.clone(), path, &body, Some("secretX")).await.0,
        StatusCode::UNAUTHORIZED
    );
    assert_eq!(
        request(app.clone(), path, "{", Some("secret")).await.0,
        StatusCode::BAD_REQUEST
    );
    assert_eq!(
        request(app.clone(), path, &body, Some("secret")).await.1["code"],
        0
    );
    assert_eq!(
        request(app.clone(), path, &body, Some("secret")).await.0,
        StatusCode::CONFLICT
    );
    let insert = json!({"collectionName":"items","data":[{"id":1,"vector":[1,0]}]}).to_string();
    assert_eq!(
        request(
            app.clone(),
            "/v2/vectordb/entities/insert",
            &insert,
            Some("secret")
        )
        .await
        .1["data"]["insertCount"],
        1
    );
    let search = json!({"collectionName":"items","data":[[1,0]]}).to_string();
    assert_eq!(
        request(
            app.clone(),
            "/v2/vectordb/entities/search",
            &search,
            Some("secret")
        )
        .await
        .1["data"][0]["id"],
        1
    );
    assert_eq!(
        request(app, "/v2/vectordb/users/create", "{}", Some("secret"))
            .await
            .0,
        StatusCode::NOT_IMPLEMENTED
    );
}
#[tokio::test]
async fn concurrent_insertions_are_serialized() {
    let app = router(AppState::new(Engine::memory().unwrap(), None));
    request(
        app.clone(),
        "/v2/vectordb/collections/create",
        &json!({"collectionName":"items","dimension":2,"autoID":true}).to_string(),
        None,
    )
    .await;
    let mut tasks = Vec::new();
    for _ in 0..16 {
        let app = app.clone();
        tasks.push(tokio::spawn(async move {
            request(
                app,
                "/v2/vectordb/entities/insert",
                &json!({"collectionName":"items","data":[{"vector":[1,0]}]}).to_string(),
                None,
            )
            .await
        }));
    }
    let mut ids = std::collections::BTreeSet::new();
    for task in tasks {
        let (status, v) = task.await.unwrap();
        assert_eq!(status, StatusCode::OK);
        ids.insert(v["data"]["insertIds"][0].as_i64().unwrap());
    }
    assert_eq!(ids.len(), 16);
}
