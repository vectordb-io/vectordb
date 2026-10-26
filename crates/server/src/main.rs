use clap::{Parser, Subcommand};
use std::{net::SocketAddr, path::PathBuf};
use vectordb_engine::Engine;
use vectordb_server::{AppState, router};
#[derive(Parser)]
#[command(version, about = "VectorDB — a Rust vector database")]
struct Args {
    #[command(subcommand)]
    command: Command,
}
#[derive(Subcommand)]
enum Command {
    /// Run the HTTP server. Default bind is local only.
    Serve {
        #[arg(long, env = "VECTORDB_BIND", default_value = "127.0.0.1:19530")]
        bind: SocketAddr,
        #[arg(long, env = "VECTORDB_DATA", default_value = "data/vectordb.sqlite")]
        data: PathBuf,
        #[arg(long, env = "VECTORDB_API_KEY", hide_env_values = true)]
        api_key: Option<String>,
    },
    /// Import JSON Lines while the server is stopped. Each batch commits independently.
    Import {
        #[arg(long, default_value = "data/vectordb.sqlite")]
        data: PathBuf,
        #[arg(long, default_value = "default")]
        database: String,
        #[arg(long)]
        collection: String,
        #[arg(long)]
        input: PathBuf,
        #[arg(long)]
        upsert: bool,
        #[arg(long, default_value_t = 1000)]
        batch_size: usize,
    },
    /// Export entities as JSON Lines while the server is stopped. Never overwrites output.
    Export {
        #[arg(long, default_value = "data/vectordb.sqlite")]
        data: PathBuf,
        #[arg(long, default_value = "default")]
        database: String,
        #[arg(long)]
        collection: String,
        #[arg(long)]
        output: PathBuf,
    },
    /// Make a consistent snapshot while the server is stopped.
    Backup {
        #[arg(long, default_value = "data/vectordb.sqlite")]
        data: PathBuf,
        #[arg(long)]
        output: PathBuf,
    },
}
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env().unwrap_or_else(|_| "info".into()),
        )
        .init();
    match Args::parse().command {
        Command::Import {
            data,
            database,
            collection,
            input,
            upsert,
            batch_size,
        } => {
            if !data.exists() {
                return Err("source database does not exist".into());
            }
            let input = std::io::BufReader::new(std::fs::File::open(input)?);
            let count = Engine::open(data)?.import_jsonl(
                &database,
                &collection,
                input,
                upsert,
                batch_size,
            )?;
            println!("Imported {count} entities");
        }
        Command::Export {
            data,
            database,
            collection,
            output,
        } => {
            if !data.exists() {
                return Err("source database does not exist".into());
            }
            let engine = Engine::open(data)?;
            let file = std::fs::OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(&output)?;
            let result = engine.export_jsonl(&database, &collection, std::io::BufWriter::new(file));
            if result.is_err() {
                let _ = std::fs::remove_file(&output);
            }
            println!("Exported {} entities", result?);
        }
        Command::Backup { data, output } => {
            if !data.exists() {
                return Err("source database does not exist".into());
            }
            Engine::open(data)?.backup(&output)?;
            println!("Snapshot saved to {}", output.display());
        }
        Command::Serve {
            bind,
            data,
            api_key,
        } => {
            if api_key.as_ref().is_some_and(|s| s.is_empty()) {
                return Err("API key cannot be empty".into());
            }
            let engine = Engine::open(data)?;
            let listener = tokio::net::TcpListener::bind(bind).await?;
            tracing::info!(address=%listener.local_addr()?,authentication=api_key.is_some(),"VectorDB listening");
            axum::serve(listener, router(AppState::new(engine, api_key)))
                .with_graceful_shutdown(shutdown())
                .await?;
        }
    }
    Ok(())
}
async fn shutdown() {
    let ctrl_c = async {
        tokio::signal::ctrl_c()
            .await
            .expect("install Ctrl-C handler");
    };
    #[cfg(unix)]
    let terminate = async {
        tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())
            .expect("install SIGTERM handler")
            .recv()
            .await;
    };
    #[cfg(not(unix))]
    let terminate = std::future::pending::<()>();
    tokio::select! {_=ctrl_c=>{},_=terminate=>{}}
    tracing::info!("shutdown requested; draining requests");
}
