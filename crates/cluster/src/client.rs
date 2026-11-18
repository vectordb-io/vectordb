//! Synchronous client for the public cluster HTTP API. Reuse the key on uncertain writes.
use crate::{Error, Result};
use serde_json::Value;

pub struct Client {
    http: reqwest::blocking::Client,
    origin: reqwest::Url,
    token: String,
}
impl Client {
    pub fn new(origin: &str, token: &str) -> Result<Self> {
        let url = reqwest::Url::parse(origin)
            .map_err(|_| Error::Invalid("invalid cluster origin".into()))?;
        if !matches!(url.scheme(), "http" | "https")
            || url.path() != "/"
            || !url.username().is_empty()
            || url.password().is_some()
            || url.query().is_some()
            || url.fragment().is_some()
        {
            return Err(Error::Invalid("HTTP(S) origin required".into()));
        }
        Ok(Self {
            http: reqwest::blocking::Client::builder()
                .timeout(std::time::Duration::from_secs(45))
                .redirect(reqwest::redirect::Policy::none())
                .build()?,
            origin: url,
            token: token.into(),
        })
    }
    pub fn execute(&self, operation: &str, body: Value, key: Option<&str>) -> Result<Value> {
        let (category, action) = operation
            .split_once('/')
            .ok_or_else(|| Error::Invalid("expected category/action".into()))?;
        crate::identifier(category)?;
        crate::identifier(action)?;
        let mut url = self.origin.clone();
        url.set_path(&format!("/v2/vectordb/{operation}"));
        let mut req = self.http.post(url).bearer_auth(&self.token).json(&body);
        if let Some(key) = key {
            req = req.header("Idempotency-Key", key);
        }
        let reply = req.send()?;
        let status = reply.status();
        let envelope: Value = reply.json()?;
        if !status.is_success() || envelope["code"] != 0 {
            return Err(Error::Remote(
                status.as_u16(),
                envelope["message"]
                    .as_str()
                    .unwrap_or("invalid response")
                    .into(),
            ));
        }
        Ok(envelope)
    }
    pub fn new_key() -> String {
        crate::state::idempotency_key(
            crate::state::now_ms(),
            &uuid::Uuid::new_v4().simple().to_string(),
        )
    }
}
