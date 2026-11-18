//! Test client for real TLS/protocol rejection checks. No private key material is logged.
use tonic::transport::{Certificate, ClientTlsConfig, Endpoint, Identity};
use vectordb_cluster::{
    state::now_ms,
    transport::wire::{CallRequest, cluster_client::ClusterClient},
};
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a = std::env::args().skip(1).collect::<Vec<_>>();
    if a.len() != 10 {
        return Err("usage: rpc_probe endpoint server-id ca cert-or-- key cluster sender incarnation version path".into());
    }
    let mut tls = ClientTlsConfig::new()
        .ca_certificate(Certificate::from_pem(std::fs::read(&a[2])?))
        .domain_name(&a[1]);
    if a[3] != "-" {
        tls = tls.identity(Identity::from_pem(
            std::fs::read(&a[3])?,
            std::fs::read(&a[4])?,
        ));
    }
    let result = async {
        let channel = Endpoint::from_shared(a[0].clone())?
            .tls_config(tls)?
            .connect_timeout(std::time::Duration::from_secs(3))
            .timeout(std::time::Duration::from_secs(5))
            .connect()
            .await?;
        let response = ClusterClient::new(channel)
            .call(CallRequest {
                version: a[8].parse()?,
                cluster: a[5].clone(),
                sender: a[6].clone(),
                incarnation: a[7].clone(),
                path: a[9].clone(),
                body: b"{}".to_vec(),
                idempotency_key: String::new(),
                deadline_ms: now_ms() + 5000,
            })
            .await?
            .into_inner();
        let value: serde_json::Value = serde_json::from_slice(&response.body)?;
        Ok::<_, Box<dyn std::error::Error>>(value)
    }
    .await;
    match result {
        Ok(value) => println!(
            "{}",
            serde_json::json!({"accepted":value["code"]==0,"response":value})
        ),
        Err(e) => println!(
            "{}",
            serde_json::json!({"accepted":false,"rpcError":e.to_string()})
        ),
    }
    Ok(())
}
