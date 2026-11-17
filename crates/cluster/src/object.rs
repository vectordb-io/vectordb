use crate::{Error, Result, digest, identifier};
use chrono::Utc;
use hmac::{Hmac, Mac};
use reqwest::{Method, blocking::Client};
use sha2::Sha256;
use std::{io::Read, time::Duration};

pub const MAX_CHECKPOINT_BYTES: usize = 64 * 1024 * 1024;
#[derive(Clone)]
pub struct Objects {
    client: Client,
    endpoint: String,
    bucket: String,
    region: String,
    access_key: String,
    secret_key: String,
}
impl Objects {
    pub fn new(
        endpoint: String,
        bucket: String,
        region: String,
        access_key: String,
        secret_key: String,
    ) -> Result<Self> {
        identifier(&bucket)?;
        identifier(&region)?;
        let url = reqwest::Url::parse(&endpoint)
            .map_err(|_| Error::Invalid("invalid S3 endpoint".into()))?;
        if !matches!(url.scheme(), "http" | "https")
            || url.path() != "/"
            || url.query().is_some()
            || !url.username().is_empty()
            || url.password().is_some()
            || url.fragment().is_some()
        {
            return Err(Error::Invalid(
                "S3 endpoint must be an HTTP(S) origin".into(),
            ));
        }
        Ok(Self {
            client: Client::builder()
                .timeout(Duration::from_secs(30))
                .redirect(reqwest::redirect::Policy::none())
                .build()?,
            endpoint: endpoint.trim_end_matches('/').into(),
            bucket,
            region,
            access_key,
            secret_key,
        })
    }
    fn request(
        &self,
        method: Method,
        key: &str,
        body: Vec<u8>,
    ) -> Result<reqwest::blocking::Response> {
        if key.split('/').any(|p| {
            p.is_empty()
                || p == "."
                || p == ".."
                || !p
                    .bytes()
                    .all(|b| b.is_ascii_alphanumeric() || b"-_.".contains(&b))
        }) {
            return Err(Error::Invalid("invalid object key".into()));
        }
        let url = reqwest::Url::parse(&format!("{}/{}/{}", self.endpoint, self.bucket, key))
            .map_err(|_| Error::Invalid("invalid object URL".into()))?;
        let host = match url.port() {
            Some(port) => format!("{}:{port}", url.host_str().unwrap_or_default()),
            None => url.host_str().unwrap_or_default().into(),
        };
        let now = Utc::now();
        let date = now.format("%Y%m%d").to_string();
        let timestamp = now.format("%Y%m%dT%H%M%SZ").to_string();
        let hash = digest(&body);
        let headers = format!("host:{host}\nx-amz-content-sha256:{hash}\nx-amz-date:{timestamp}\n");
        let signed = "host;x-amz-content-sha256;x-amz-date";
        let canonical = format!(
            "{}\n{}\n\n{headers}\n{signed}\n{hash}",
            method.as_str(),
            url.path()
        );
        let scope = format!("{date}/{}/s3/aws4_request", self.region);
        let message = format!(
            "AWS4-HMAC-SHA256\n{timestamp}\n{scope}\n{}",
            digest(canonical.as_bytes())
        );
        let key = sign(
            format!("AWS4{}", self.secret_key).as_bytes(),
            date.as_bytes(),
        );
        let key = sign(&key, self.region.as_bytes());
        let key = sign(&key, b"s3");
        let key = sign(&key, b"aws4_request");
        let signature = sign(&key, message.as_bytes())
            .iter()
            .map(|b| format!("{b:02x}"))
            .collect::<String>();
        let auth = format!(
            "AWS4-HMAC-SHA256 Credential={}/{scope}, SignedHeaders={signed}, Signature={signature}",
            self.access_key
        );
        let mut request = self
            .client
            .request(method.clone(), url)
            .header("x-amz-date", timestamp)
            .header("x-amz-content-sha256", hash)
            .header("authorization", auth);
        if method == Method::PUT {
            request = request.header("If-None-Match", "*");
        }
        Ok(request.body(body).send()?.error_for_status()?)
    }
    pub fn put(&self, key: &str, bytes: Vec<u8>) -> Result<()> {
        if bytes.len() > MAX_CHECKPOINT_BYTES {
            return Err(Error::Invalid(
                "P1 checkpoint exceeds 64 MiB safety limit".into(),
            ));
        }
        self.request(Method::PUT, key, bytes)?;
        Ok(())
    }
    pub fn get(&self, key: &str) -> Result<Vec<u8>> {
        let response = self.request(Method::GET, key, vec![])?;
        let mut bytes = vec![];
        response
            .take(MAX_CHECKPOINT_BYTES as u64 + 1)
            .read_to_end(&mut bytes)?;
        if bytes.len() > MAX_CHECKPOINT_BYTES {
            return Err(Error::Integrity("checkpoint is too large".into()));
        }
        Ok(bytes)
    }
}
fn sign(key: &[u8], data: &[u8]) -> Vec<u8> {
    let mut mac = Hmac::<Sha256>::new_from_slice(key).expect("HMAC accepts any key length");
    mac.update(data);
    mac.finalize().into_bytes().to_vec()
}
