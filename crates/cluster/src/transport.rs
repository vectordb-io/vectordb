//! Versioned protobuf RPC with mandatory mutual TLS and leased service identity.
use crate::{
    Error, Result,
    control::{Control, Member, PROTOCOL},
    state::now_ms,
};
use serde_json::Value;
use std::{
    sync::{Arc, OnceLock},
    time::Duration,
};
use tonic::{
    Request, Response, Status,
    transport::{Certificate, ClientTlsConfig, Endpoint, Identity, Server, ServerTlsConfig},
};
pub mod wire {
    tonic::include_proto!("vectordb.internal.rpc.v1");
}
use wire::{
    CallRequest, CallResponse,
    cluster_client::ClusterClient,
    cluster_server::{Cluster, ClusterServer},
};
pub const MAX_RPC: usize = 32 * 1024 * 1024 + 1024;
#[derive(Clone)]
pub struct Tls {
    ca: Vec<u8>,
    cert: Vec<u8>,
    key: Vec<u8>,
}
impl Tls {
    pub fn load(ca: &str, cert: &str, key: &str, id: &str) -> Result<Self> {
        let this = Self {
            ca: std::fs::read(ca)?,
            cert: std::fs::read(cert)?,
            key: std::fs::read(key)?,
        };
        let (_, pem) = x509_parser::pem::parse_x509_pem(&this.cert)
            .map_err(|_| Error::Invalid("invalid service certificate".into()))?;
        verify_identity(&pem.contents, id)?;
        Ok(this)
    }
    fn identity(&self) -> Identity {
        Identity::from_pem(&self.cert, &self.key)
    }
    pub fn server(&self) -> ServerTlsConfig {
        ServerTlsConfig::new()
            .identity(self.identity())
            .client_ca_root(Certificate::from_pem(&self.ca))
            .client_auth_optional(false)
            .timeout(Duration::from_secs(5))
    }
}
pub fn verify_identity(der: &[u8], id: &str) -> Result<()> {
    use x509_parser::{extensions::GeneralName, prelude::*};
    let (_, certificate) = X509Certificate::from_der(der)
        .map_err(|_| Error::Invalid("invalid peer certificate".into()))?;
    let matches = certificate
        .subject_alternative_name()
        .ok()
        .flatten()
        .is_some_and(|san| {
            san.value
                .general_names
                .iter()
                .any(|name| matches!(name, GeneralName::DNSName(n) if *n == id))
        });
    if !matches {
        return Err(Error::Invalid(
            "certificate SAN does not match node identity".into(),
        ));
    }
    Ok(())
}
#[derive(Clone)]
pub struct Client {
    pub control: Control,
    pub cluster: String,
    pub sender: String,
    pub incarnation: String,
    pub tls: Tls,
}
fn runtime() -> &'static tokio::runtime::Runtime {
    static RUNTIME: OnceLock<tokio::runtime::Runtime> = OnceLock::new();
    RUNTIME.get_or_init(|| {
        tokio::runtime::Builder::new_multi_thread()
            .worker_threads(2)
            .enable_all()
            .build()
            .expect("RPC runtime")
    })
}
impl Client {
    /// Used only from blocking workers. Public HTTP addresses are identifiers;
    /// transport always resolves the registered TLS endpoint and service SAN.
    pub fn call(
        &self,
        address: &str,
        body: Value,
        key: Option<&str>,
        deadline_ms: i64,
    ) -> Result<Value> {
        self.control
            .verify_member(&self.sender, &self.incarnation)?;
        let url = reqwest::Url::parse(address)
            .map_err(|_| Error::Invalid("invalid internal target".into()))?;
        let origin = url.origin().ascii_serialization();
        let member = self
            .control
            .members()?
            .into_values()
            .find(|m| m.origin == origin)
            .ok_or_else(|| Error::Unavailable("RPC target is not a live member".into()))?;
        let request = CallRequest {
            version: PROTOCOL,
            cluster: self.cluster.clone(),
            sender: self.sender.clone(),
            incarnation: self.incarnation.clone(),
            path: url.path().into(),
            body: serde_json::to_vec(&body)?,
            idempotency_key: key.unwrap_or_default().into(),
            deadline_ms,
        };
        let timeout = deadline_ms - now_ms();
        if timeout <= 0 || request.body.len() > 1024 * 1024 {
            return Err(Error::Unavailable(
                "RPC deadline/request budget exceeded".into(),
            ));
        }
        runtime().block_on(async {
            let timeout = Duration::from_millis(timeout as u64);
            let endpoint = Endpoint::from_shared(member.rpc)
                .map_err(|e| Error::Invalid(e.to_string()))?
                .tls_config(
                    ClientTlsConfig::new()
                        .ca_certificate(Certificate::from_pem(&self.tls.ca))
                        .identity(self.tls.identity())
                        .domain_name(member.id),
                )
                .map_err(|e| Error::Invalid(e.to_string()))?
                .connect_timeout(timeout.min(Duration::from_secs(5)))
                .timeout(timeout);
            let result = tokio::time::timeout(timeout, async {
                let channel = endpoint
                    .connect()
                    .await
                    .map_err(|e| Error::Unavailable(e.to_string()))?;
                let response = ClusterClient::new(channel)
                    .max_decoding_message_size(MAX_RPC)
                    .max_encoding_message_size(1024 * 1024 + 1024)
                    .call(request)
                    .await
                    .map_err(|e| Error::Unavailable(e.to_string()))?
                    .into_inner();
                if response.version != PROTOCOL {
                    return Err(Error::Unsupported("RPC response protocol mismatch".into()));
                }
                Ok(serde_json::from_slice(&response.body)?)
            })
            .await;
            result.map_err(|_| Error::Unavailable("RPC deadline exceeded".into()))?
        })
    }
}
pub trait Handler: Send + Sync + 'static {
    fn execute(&self, request: CallRequest, peer: Member) -> Result<Value>;
}
#[derive(Clone)]
struct Rpc {
    handler: Arc<dyn Handler>,
    control: Control,
    cluster: String,
    id: String,
    incarnation: String,
    slots: Arc<tokio::sync::Semaphore>,
}
#[tonic::async_trait]
impl Cluster for Rpc {
    async fn call(
        &self,
        request: Request<CallRequest>,
    ) -> std::result::Result<Response<CallResponse>, Status> {
        let certificates = request
            .peer_certs()
            .ok_or_else(|| Status::unauthenticated("client certificate required"))?;
        let certificate = certificates
            .first()
            .ok_or_else(|| Status::unauthenticated("empty client certificate"))?;
        let body = request.get_ref();
        verify_identity(certificate.as_ref(), &body.sender)
            .map_err(|_| Status::unauthenticated("service identity mismatch"))?;
        if body.cluster != self.cluster || body.version != PROTOCOL {
            return Err(Status::failed_precondition("incompatible cluster/protocol"));
        }
        if body.body.len() > 1024 * 1024
            || body.deadline_ms <= now_ms()
            || body.deadline_ms > now_ms() + 45_000
        {
            return Err(Status::invalid_argument("RPC budget/deadline invalid"));
        }
        let permit = self
            .slots
            .clone()
            .try_acquire_owned()
            .map_err(|_| Status::resource_exhausted("RPC queue full"))?;
        let this = self.clone();
        let request = request.into_inner();
        let timeout = Duration::from_millis((request.deadline_ms - now_ms()).max(1) as u64);
        let value = tokio::time::timeout(
            timeout,
            tokio::task::spawn_blocking(move || {
                let _permit = permit;
                this.control.verify_member(&this.id, &this.incarnation)?;
                let peer = this
                    .control
                    .verify_member(&request.sender, &request.incarnation)?;
                this.handler.execute(request, peer)
            }),
        )
        .await
        .map_err(|_| Status::deadline_exceeded("RPC deadline"))?
        .map_err(|_| Status::internal("RPC worker failed"))?;
        // Preserve structured application errors for the external API.
        let value = match value {
            Ok(value) => value,
            Err(error) => {
                serde_json::json!({"code":error_code(&error),"status":error_code(&error),"message":error.to_string()})
            }
        };
        let body = serde_json::to_vec(&value).map_err(|_| Status::internal("response encoding"))?;
        if body.len() > MAX_RPC - 1024 {
            return Err(Status::resource_exhausted("RPC response budget"));
        }
        Ok(Response::new(CallResponse {
            version: PROTOCOL,
            body,
        }))
    }
}
pub fn error_code(error: &Error) -> u16 {
    match error {
        Error::Invalid(_) => 400,
        Error::Conflict(_) => 409,
        Error::Unsupported(_) => 501,
        Error::Engine(vectordb_engine::Error::Invalid(_)) => 400,
        Error::Engine(vectordb_engine::Error::Conflict(_)) => 409,
        Error::Engine(vectordb_engine::Error::NotFound(_)) => 404,
        Error::Engine(vectordb_engine::Error::Unsupported(_)) => 501,
        _ => 503,
    }
}
pub async fn serve(
    bind: std::net::SocketAddr,
    client: Client,
    handler: Arc<dyn Handler>,
    shutdown: impl std::future::Future<Output = ()> + Send + 'static,
) -> Result<()> {
    Server::builder()
        .tls_config(client.tls.server())
        .map_err(|e| Error::Invalid(e.to_string()))?
        .add_service(
            ClusterServer::new(Rpc {
                handler,
                control: client.control,
                cluster: client.cluster,
                id: client.sender,
                incarnation: client.incarnation,
                slots: Arc::new(tokio::sync::Semaphore::new(32)),
            })
            .max_decoding_message_size(1024 * 1024 + 1024)
            .max_encoding_message_size(MAX_RPC),
        )
        .serve_with_shutdown(bind, shutdown)
        .await
        .map_err(|e| Error::Unavailable(e.to_string()))
}
