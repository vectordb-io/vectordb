use crate::{Error, Result, state::Scope};
use base64::{Engine, engine::general_purpose::STANDARD};
use reqwest::blocking::Client;
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{io::BufRead, time::Duration};

#[derive(Clone)]
pub struct Catalog {
    client: Client,
    endpoint: String,
    prefix: String,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Owner {
    pub nonce: String,
    pub address: String,
}
#[derive(Clone, Debug)]
pub struct Lease {
    pub id: String,
    pub epoch: i64,
    pub owner: Owner,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Manifest {
    pub scope: Scope,
    pub offset: i64,
    pub key: String,
    pub sha256: String,
    pub bytes: usize,
}
impl Catalog {
    pub fn new(endpoint: String, scope: &Scope) -> Result<Self> {
        Ok(Self {
            client: Client::builder().timeout(Duration::from_secs(10)).build()?,
            endpoint: endpoint.trim_end_matches('/').into(),
            prefix: format!("/vectordb/{}/", scope.prefix()),
        })
    }
    fn key(&self, name: &str) -> String {
        STANDARD.encode(format!("{}{name}", self.prefix))
    }
    fn post(&self, path: &str, body: Value) -> Result<Value> {
        let response: Value = self
            .client
            .post(format!("{}{path}", self.endpoint))
            .json(&body)
            .send()?
            .error_for_status()?
            .json()?;
        if response.get("error").is_some() {
            return Err(Error::Unavailable("etcd returned an error".into()));
        }
        Ok(response)
    }
    fn get(&self, name: &str) -> Result<Option<(String, Vec<u8>)>> {
        let response = self.post(
            "/v3/kv/range",
            json!({"key":self.key(name),"serializable":false}),
        )?;
        let Some(kv) = response["kvs"].as_array().and_then(|v| v.first()) else {
            return Ok(None);
        };
        let bytes = STANDARD
            .decode(
                kv["value"]
                    .as_str()
                    .ok_or_else(|| Error::Integrity("missing catalog value".into()))?,
            )
            .map_err(|_| Error::Integrity("invalid catalog base64".into()))?;
        Ok(Some((
            kv["mod_revision"].as_str().unwrap_or_default().into(),
            bytes,
        )))
    }
    pub fn acquire(&self, address: String) -> Result<Lease> {
        let grant = self.post("/v3/lease/grant", json!({"TTL":30}))?;
        let id = grant["ID"]
            .as_str()
            .ok_or_else(|| Error::Unavailable("lease grant missing ID".into()))?
            .to_string();
        let owner = Owner {
            nonce: uuid::Uuid::new_v4().to_string(),
            address,
        };
        let result = self.post("/v3/kv/txn", json!({
            "compare":[{"key":self.key("owner"),"target":"VERSION","version":"0","result":"EQUAL"}],
            "success":[{"request_put":{"key":self.key("owner"),"value":STANDARD.encode(serde_json::to_vec(&owner)?),"lease":id}}],"failure":[]
        }))?;
        if result["succeeded"] != true {
            let _ = self.revoke(&id);
            return Err(Error::Conflict("collection already has a writer".into()));
        }
        let epoch = result["header"]["revision"]
            .as_str()
            .and_then(|s| s.parse().ok())
            .ok_or_else(|| Error::Integrity("missing ownership revision".into()))?;
        Ok(Lease { id, epoch, owner })
    }
    pub fn owner(&self) -> Result<Owner> {
        let (_, bytes) = self
            .get("owner")?
            .ok_or_else(|| Error::Unavailable("collection has no writer".into()))?;
        Ok(serde_json::from_slice(&bytes)?)
    }
    pub fn verify(&self, lease: &Lease) -> Result<()> {
        if self.owner()?.nonce != lease.owner.nonce {
            return Err(Error::Unavailable("ownership lease lost".into()));
        }
        Ok(())
    }
    pub fn renew(&self, lease: &Lease) -> Result<()> {
        // The HTTP gateway streams responses: read exactly one line, then close.
        let response = self
            .client
            .post(format!("{}/v3/lease/keepalive", self.endpoint))
            .json(&json!({"ID":lease.id}))
            .send()?
            .error_for_status()?;
        let mut line = String::new();
        std::io::BufReader::new(response).read_line(&mut line)?;
        let value: Value = serde_json::from_str(&line)?;
        let ttl = value["result"]["TTL"]
            .as_str()
            .and_then(|s| s.parse::<i64>().ok())
            .unwrap_or(0);
        if ttl <= 0 {
            return Err(Error::Unavailable("ownership lease expired".into()));
        }
        self.verify(lease)
    }
    pub fn revoke(&self, id: &str) -> Result<()> {
        self.post("/v3/lease/revoke", json!({"ID":id}))?;
        Ok(())
    }
    pub fn manifest(&self) -> Result<Option<Manifest>> {
        self.get("manifest")?
            .map(|(_, bytes)| serde_json::from_slice(&bytes).map_err(Error::from))
            .transpose()
    }
    pub fn publish(&self, manifest: &Manifest) -> Result<bool> {
        let old = self.get("manifest")?;
        if let Some((_, bytes)) = &old {
            let previous: Manifest = serde_json::from_slice(bytes)?;
            if previous.scope != manifest.scope {
                return Err(Error::Integrity("manifest scope mismatch".into()));
            }
            if previous.offset >= manifest.offset {
                return Ok(false);
            }
        }
        let compare = match old {
            None => {
                json!({"key":self.key("manifest"),"target":"VERSION","version":"0","result":"EQUAL"})
            }
            Some((revision, _)) => {
                json!({"key":self.key("manifest"),"target":"MOD","mod_revision":revision,"result":"EQUAL"})
            }
        };
        let result = self.post("/v3/kv/txn", json!({"compare":[compare],"success":[{"request_put":{"key":self.key("manifest"),"value":STANDARD.encode(serde_json::to_vec(manifest)?)}}],"failure":[]}))?;
        Ok(result["succeeded"] == true)
    }
}
