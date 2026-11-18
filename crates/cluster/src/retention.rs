//! Terminal generation retirement. No new reader pin, writer lease or checkpoint
//! can be installed after the same transaction removes the directory identity.
use crate::{
    Error, Result,
    catalog::Catalog,
    directory::{Directory, Lifecycle},
    kafka::Log,
    object::Objects,
    state::{IDEMPOTENCY_WINDOW_MS, Scope},
};
use base64::{Engine, engine::general_purpose::STANDARD};
use serde_json::json;

impl Directory {
    pub fn stage_restore(&self, lease: &crate::catalog::Lease, scope: &Scope) -> Result<()> {
        scope.validate()?;
        let key = self.store.key(&format!("stage/{}", scope.generation));
        let r=self.store.post("/v3/kv/txn",json!({"compare":[self.compare("0"),{"key":self.store.key("owner"),"target":"VALUE","value":STANDARD.encode(serde_json::to_vec(&lease.owner)?)}],"success":[{"request_put":{"key":key,"value":STANDARD.encode(serde_json::to_vec(scope)?)}}]}))?;
        if r["succeeded"] != true {
            return Err(Error::Conflict("restore lost its reservation".into()));
        }
        Ok(())
    }
    /// A terminated restore may leave verified but unpublished topics and snapshots.
    /// Reconcile against the final catalog after its exclusive lease is released.
    pub fn retire_staging(&self, now: i64) -> Result<()> {
        for (key, bytes) in self.store.range("stage/")?.into_iter().take(16) {
            let scope: Scope = serde_json::from_slice(&bytes)?;
            let (revision, state) = self.load()?;
            let mut success = vec![json!({"request_delete_range":{"key":STANDARD.encode(&key)}})];
            if !state
                .collections
                .get(&scope.collection)
                .is_some_and(|c| c.scope == scope)
            {
                let catalog = Catalog::new(self.store.endpoint().into(), &scope)?;
                success.push(json!({"request_put":{"key":catalog.key("closed"),"value":STANDARD.encode(now.to_string())}}));
                success.push(json!({"request_put":{"key":self.store.key(&format!("purge/{}",scope.generation)),"value":STANDARD.encode(&bytes)}}));
            }
            self.store.post("/v3/kv/txn",json!({"compare":[self.compare(&revision),{"key":STANDARD.encode(&key),"target":"VALUE","value":STANDARD.encode(&bytes)},{"key":self.store.key("owner"),"target":"VERSION","version":"0"}],"success":success}))?;
        }
        Ok(())
    }
    pub fn retire_dropped(&self, now: i64) -> Result<()> {
        let (revision, mut state) = self.load()?;
        let Some(c) = state.collections.values_mut().find(|c| {
            c.state == Lifecycle::Dropped
                && c.dropped_at_ms
                    .is_none_or(|at| now.saturating_sub(at) > IDEMPOTENCY_WINDOW_MS + 120_000)
        }) else {
            return Ok(());
        };
        let scope = c.scope.clone();
        let mut success = vec![];
        if c.dropped_at_ms.is_none() {
            // Pre-M6 tombstones receive a full retry window from first observation.
            c.dropped_at_ms = Some(now);
        } else {
            state.collections.remove(&scope.collection);
            let catalog = Catalog::new(self.store.endpoint().into(), &scope)?;
            success.push(json!({"request_put":{"key":catalog.key("closed"),"value":STANDARD.encode(now.to_string())}}));
            success.push(json!({"request_delete_range":{"key":catalog.key("owner")}}));
            success.push(json!({"request_put":{"key":self.store.key(&format!("purge/{}",scope.collection)),"value":STANDARD.encode(serde_json::to_vec(&scope)?)}}));
        }
        success.push(self.put(&state)?);
        self.store.post("/v3/kv/txn",json!({"compare":[self.compare(&revision),{"key":self.store.key("owner"),"target":"VERSION","version":"0"}],"success":success}))?;
        Ok(())
    }
    pub fn purge_retired(&self, objects: &Objects, brokers: &str, now: i64) -> Result<()> {
        for (key, bytes) in self.store.range("purge/")?.into_iter().take(1) {
            let scope: Scope = serde_json::from_slice(&bytes)?;
            let catalog = Catalog::new(self.store.endpoint().into(), &scope)?;
            let (_, closed) = catalog
                .get("closed")?
                .ok_or_else(|| Error::Integrity("missing retirement fence".into()))?;
            if now.saturating_sub(serde_json::from_slice(&closed)?) < 300_000
                || !catalog.range("pins/")?.is_empty()
            {
                return Ok(());
            }
            let prefix = format!("{}/checkpoints/", scope.prefix());
            objects.visit_checkpoint_objects(&prefix, |k, _, _| objects.delete(k))?;
            Log::reader(brokers.into(), &scope).remove_closed_topic()?;
            for (k, _) in catalog.range("retired/")? {
                catalog.post("/v3/kv/deleterange", json!({"key":STANDARD.encode(k)}))?;
            }
            catalog.post("/v3/kv/txn",json!({"success":[{"request_delete_range":{"key":catalog.key("manifest")}},{"request_delete_range":{"key":STANDARD.encode(key)}}]}))?;
        }
        Ok(())
    }
}
