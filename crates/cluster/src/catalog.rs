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
    #[serde(default)]
    pub phase: OwnerPhase,
}
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum OwnerPhase {
    Assigning,
    Recovering,
    #[default]
    Active,
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
    #[serde(default)]
    pub objects: Vec<String>,
}
impl Catalog {
    pub fn new(endpoint: String, scope: &Scope) -> Result<Self> {
        Ok(Self {
            client: Client::builder().timeout(Duration::from_secs(10)).build()?,
            endpoint: endpoint.trim_end_matches('/').into(),
            prefix: format!("/vectordb/{}/", scope.prefix()),
        })
    }
    pub(crate) fn endpoint(&self) -> &str {
        &self.endpoint
    }
    pub(crate) fn key(&self, name: &str) -> String {
        STANDARD.encode(format!("{}{name}", self.prefix))
    }
    pub(crate) fn post(&self, path: &str, body: Value) -> Result<Value> {
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
    pub(crate) fn get(&self, name: &str) -> Result<Option<(String, Vec<u8>)>> {
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
    fn open_compare(&self) -> Value {
        json!({"key":self.key("closed"),"target":"VERSION","version":"0"})
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
            phase: OwnerPhase::Assigning,
        };
        let result = self.post("/v3/kv/txn", json!({
            "compare":[self.open_compare(),{"key":self.key("owner"),"target":"VERSION","version":"0","result":"EQUAL"}],
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
    pub fn active_owner(&self) -> Result<Owner> {
        let owner = self.owner()?;
        if owner.phase != OwnerPhase::Active {
            return Err(Error::Unavailable("writer recovery is incomplete".into()));
        }
        Ok(owner)
    }
    pub fn phase(&self, lease: &Lease, next: OwnerPhase) -> Result<()> {
        let (revision, bytes) = self
            .get("owner")?
            .ok_or_else(|| Error::Unavailable("ownership lost".into()))?;
        let mut owner: Owner = serde_json::from_slice(&bytes)?;
        if owner.nonce != lease.owner.nonce
            || !matches!(
                (owner.phase, next),
                (OwnerPhase::Assigning, OwnerPhase::Recovering)
                    | (OwnerPhase::Recovering, OwnerPhase::Active)
            )
        {
            return Err(Error::Conflict("invalid owner transition".into()));
        }
        owner.phase = next;
        let result = self.post("/v3/kv/txn", json!({"compare":[{"key":self.key("owner"),"target":"MOD","mod_revision":revision,"result":"EQUAL"}],"success":[{"request_put":{"key":self.key("owner"),"value":STANDARD.encode(serde_json::to_vec(&owner)?),"lease":lease.id}}],"failure":[]}))?;
        if result["succeeded"] != true {
            return Err(Error::Conflict("owner transition raced".into()));
        }
        Ok(())
    }
    pub fn verify(&self, lease: &Lease) -> Result<()> {
        if self.owner()?.nonce != lease.owner.nonce {
            return Err(Error::Unavailable("ownership lease lost".into()));
        }
        Ok(())
    }
    pub fn renew(&self, lease: &Lease) -> Result<()> {
        self.keepalive(&lease.id)?;
        self.verify(lease)
    }
    pub fn keepalive(&self, id: &str) -> Result<()> {
        // The HTTP gateway streams responses: read exactly one line, then close.
        let response = self
            .client
            .post(format!("{}/v3/lease/keepalive", self.endpoint))
            .json(&json!({"ID":id}))
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
        Ok(())
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
        let parent = manifest
            .key
            .rsplit_once('/')
            .map(|(p, _)| p)
            .ok_or_else(|| Error::Invalid("checkpoint key requires a namespace".into()))?;
        let required = format!(
            "{}checkpoints/",
            self.prefix.trim_start_matches("/vectordb/")
        );
        let namespace = parent.strip_prefix(&required).unwrap_or_default();
        let prefix = format!("{}-", manifest.offset);
        if manifest.offset < 0
            || format!("/vectordb/{}/", manifest.scope.prefix()) != self.prefix
            || namespace
                .strip_prefix(&prefix)
                .is_none_or(|nonce| crate::identifier(nonce).is_err())
            || !parent.starts_with(&required)
            || manifest
                .objects
                .iter()
                .any(|k| k.rsplit_once('/').map(|(p, _)| p) != Some(parent))
        {
            return Err(Error::Invalid(
                "checkpoint objects must use a unique collection namespace".into(),
            ));
        }
        let old = self.get("manifest")?;
        if let Some((_, bytes)) = &old {
            let previous: Manifest = serde_json::from_slice(bytes)?;
            if previous.scope != manifest.scope {
                return Err(Error::Integrity("manifest scope mismatch".into()));
            }
            if previous.offset >= manifest.offset {
                return Ok(false);
            }
            if previous.key.rsplit_once('/').map(|(p, _)| p) == Some(parent) {
                return Err(Error::Conflict(
                    "checkpoint namespace cannot be reused".into(),
                ));
            }
        }
        let retired = old
            .as_ref()
            .map(|(_, bytes)| serde_json::from_slice::<Manifest>(bytes))
            .transpose()?;
        let compare = match old {
            None => {
                json!({"key":self.key("manifest"),"target":"VERSION","version":"0","result":"EQUAL"})
            }
            Some((revision, _)) => {
                json!({"key":self.key("manifest"),"target":"MOD","mod_revision":revision,"result":"EQUAL"})
            }
        };
        let mut success = vec![
            json!({"request_put":{"key":self.key("manifest"),"value":STANDARD.encode(serde_json::to_vec(manifest)?)}}),
        ];
        if let Some(old) = retired {
            success.push(json!({"request_put":{"key":self.key(&format!("retired/{}",crate::digest(old.key.as_bytes()))),"value":STANDARD.encode(serde_json::to_vec(&old)?)}}));
        }
        let result = self.post(
            "/v3/kv/txn",
            json!({"compare":[self.open_compare(),compare],"success":success,"failure":[]}),
        )?;
        Ok(result["succeeded"] == true)
    }
    pub(crate) fn range(&self, name: &str) -> Result<Vec<(String, Vec<u8>)>> {
        let prefix = format!("{}{name}", self.prefix);
        let mut end = prefix.as_bytes().to_vec();
        let last = end
            .last_mut()
            .ok_or_else(|| Error::Invalid("empty prefix".into()))?;
        *last += 1;
        let response=self.post("/v3/kv/range",json!({"key":STANDARD.encode(prefix),"range_end":STANDARD.encode(end),"serializable":false}))?;
        response["kvs"]
            .as_array()
            .into_iter()
            .flatten()
            .map(|kv| {
                let decode = |field: &str| -> Result<Vec<u8>> {
                    STANDARD
                        .decode(kv[field].as_str().unwrap_or_default())
                        .map_err(|_| Error::Integrity("invalid catalog value".into()))
                };
                let key = String::from_utf8(decode("key")?)
                    .map_err(|_| Error::Integrity("invalid key".into()))?;
                Ok((key, decode("value")?))
            })
            .collect()
    }
    /// Pin registration and manifest revision check are atomic with publication.
    pub fn pin_latest(&self) -> Result<Option<(Manifest, String, String)>> {
        for _ in 0..8 {
            let Some((revision, bytes)) = self.get("manifest")? else {
                return Ok(None);
            };
            let manifest: Manifest = serde_json::from_slice(&bytes)?;
            let grant = self.post("/v3/lease/grant", json!({"TTL":30}))?;
            let lease = grant["ID"]
                .as_str()
                .ok_or_else(|| Error::Integrity("missing pin lease".into()))?
                .to_string();
            let key = format!(
                "pins/{}/{}",
                crate::digest(manifest.key.as_bytes()),
                uuid::Uuid::new_v4()
            );
            let result=self.post("/v3/kv/txn",json!({"compare":[self.open_compare(),{"key":self.key("manifest"),"target":"MOD","mod_revision":revision,"result":"EQUAL"}],"success":[{"request_put":{"key":self.key(&key),"value":STANDARD.encode(&manifest.key),"lease":lease}}],"failure":[]}))?;
            if result["succeeded"] == true {
                return Ok(Some((manifest, key, lease)));
            }
            self.revoke(&lease)?;
        }
        Err(Error::Unavailable(
            "checkpoint changed repeatedly during pin acquisition".into(),
        ))
    }
    pub fn verify_pin(&self, key: &str) -> Result<()> {
        if self.get(key)?.is_none() {
            Err(Error::Unavailable(
                "snapshot pin expired; reload latest checkpoint".into(),
            ))
        } else {
            Ok(())
        }
    }
    pub fn collect_orphans(
        &self,
        objects: &crate::object::Objects,
        before_ms: i64,
    ) -> Result<usize> {
        let Some(current) = self.manifest()? else {
            return Ok(0);
        };
        let prefix = format!("{}/checkpoints/", current.scope.prefix());
        let mut protected = std::collections::BTreeSet::new();
        for (_, bytes) in self.range("retired/")? {
            let retired: Manifest = serde_json::from_slice(&bytes)?;
            if let Some((parent, _)) = retired.key.rsplit_once('/') {
                protected.insert(parent.to_string());
            }
        }
        let mut removed = 0;
        objects.visit_checkpoint_objects(&prefix, |key, modified, _bytes| {
            if modified > before_ms {
                return Ok(());
            }
            let Some((namespace, file)) = key.strip_prefix(&prefix).and_then(|p| p.split_once('/'))
            else {
                return Ok(());
            };
            let Some((offset, nonce)) = namespace.split_once('-') else {
                return Ok(());
            };
            // A strictly older offset can never be published again, even by a paused uploader.
            if file.contains('/')
                || crate::identifier(nonce).is_err()
                || !offset
                    .parse::<i64>()
                    .is_ok_and(|n| n >= 0 && n < current.offset)
            {
                return Ok(());
            }
            if !protected.contains(&format!("{prefix}{namespace}")) {
                objects.delete(key)?;
                removed += 1;
            }
            Ok(())
        })?;
        Ok(removed)
    }
    pub fn collect_retired(&self, mut delete: impl FnMut(&str) -> Result<()>) -> Result<usize> {
        let mut count = 0;
        for (key, bytes) in self.range("retired/")? {
            let manifest: Manifest = serde_json::from_slice(&bytes)?;
            // A retired manifest can never become current again (offsets increase),
            // hence no new pin can register after the linearizable empty read.
            if !self
                .range(&format!("pins/{}/", crate::digest(manifest.key.as_bytes())))?
                .is_empty()
            {
                continue;
            }
            for object in &manifest.objects {
                delete(object)?;
            }
            delete(&manifest.key)?;
            self.post("/v3/kv/deleterange", json!({"key":STANDARD.encode(key)}))?;
            count += 1;
        }
        Ok(count)
    }
}
