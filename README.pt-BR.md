# VectorDB · Rust

**[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md) | [Español](README.es.md) | Português do Brasil**

Um banco de dados vetorial independente reimplementado em Rust, com um motor embarcável, um serviço HTTP, persistência transacional e um site estático separado.

**Status: 0.1.0 Alpha.** Inspirado no modelo de dados e no estilo de requisições REST v2 do Milvus, o VectorDB implementa um conjunto funcional de recursos para um único nó. A maior parte da infraestrutura distribuída do Milvus não está implementada, e o VectorDB não é um substituto direto do Milvus / PyMilvus. Consulte a [comparação de recursos](docs/compatibility.md) para conhecer o escopo exato. A implementação anterior em C++, sua documentação e seu site foram removidos.

## Início rápido

Requer Rust 1.89+, Cargo e ferramentas de compilação C (para compilar o SQLite incluído). No macOS, use o Xcode Command Line Tools; no Linux, o compilador C do sistema. Não precisa de um serviço de banco de dados externo.

```sh
cargo build --release --locked
./target/release/vectordb serve
```

O servidor escuta em `127.0.0.1:19530` por padrão e armazena os dados em `data/vectordb.sqlite`. Em outro terminal:

```sh
curl http://127.0.0.1:19530/healthz
bash examples/quickstart.sh
```

O exemplo cria uma coleção `demo`, insere duas entidades, busca vetores similares e aplica filtros escalares. Antes de executá-lo novamente, exclua `demo` ou use outro nome de coleção.

## Recursos atuais

- Vários bancos de dados, coleções, esquemas explícitos / criação rápida de coleções, partições, aliases de coleções, renomeação, carregamento e liberação.
- Chaves primárias Int64 / VarChar e IDs automáticos; campos escalares, JSON, Array, campos que aceitam valores nulos, valores padrão e campos dinâmicos.
- Inserção em lote / upsert de entidades completas, query / get / delete, contagem, ordenação e paginação.
- FloatVector, SparseFloatVector, BinaryVector; COSINE, IP, L2 (distância ao quadrado), HAMMING e JACCARD.
- Busca exata FLAT, IVF_FLAT, HNSW; pré-filtragem escalar, busca por intervalo, busca agrupada e múltiplos vetores de consulta.
- Fusão de buscas com múltiplos vetores: RRF e classificação ponderada normalizada.
- SQLite WAL com sincronização FULL; transações em lote, recuperação após falhas, bloqueio exclusivo de processo, backups de snapshots e importação / exportação JSONL em streaming.
- Tokens Bearer, limites de tamanho de requisição e de fila de concorrência, verificações de integridade, contadores de requisições / erros do Prometheus e encerramento controlado.

Os embeddings não são gerados automaticamente; as aplicações devem gerar e enviar os vetores. A lógica do banco de dados, os filtros, os algoritmos de indexação e o serviço HTTP são escritos em Rust. O SQLite, implementado em C, é a dependência de persistência embarcada.

## Exemplos de requisições

```sh
curl -sS localhost:19530/v2/vectordb/collections/create \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","dimension":3}'

curl -sS localhost:19530/v2/vectordb/entities/insert \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","data":[
    {"id":1,"vector":[1,0,0],"title":"Rust"},
    {"id":2,"vector":[0,1,0],"title":"Storage"}
  ]}'

curl -sS localhost:19530/v2/vectordb/entities/search \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","data":[[1,0,0]],"limit":1,"outputFields":["title"]}'
```

Resposta: `{"code":0,"data":[{"id":1,"title":"Rust","distance":1.0}]}`.

## Integração em Rust

```rust
use serde_json::json;
use vectordb_engine::Engine;

fn main() -> vectordb_engine::Result<()> {
    let mut db = Engine::memory()?;
    db.execute("collections/create", json!({"collectionName":"demo","dimension":3}))?;
    db.execute("entities/insert", json!({"collectionName":"demo","data":[
        {"id":1,"vector":[1,0,0]}
    ]}))?;
    let hits = db.execute("entities/search", json!({
        "collectionName":"demo","data":[[1,0,0]],"limit":1
    }))?;
    println!("{hits}");
    Ok(())
}
```

Execute o exemplo completo incluído no repositório: `cargo run -p vectordb-engine --example embedded`.

## Validação

```sh
cargo fmt --all --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo build --workspace --locked
python3 scripts/smoke.py
python3 scripts/check_website.py
```

Os testes cobrem reversão de transações, validação de esquemas, filtragem, precisão das chaves primárias, estado de carregamento, isolamento entre bancos de dados / partições, atualização de índices, comparações ANN, busca híbrida, snapshots, bloqueio exclusivo, corrupção do banco de dados, autenticação HTTP e gravações concorrentes. O `smoke.py` inicia um processo real com um banco de dados temporário para verificar se as gravações confirmadas podem ser recuperadas após SIGKILL. Ele não usa os dados reais de `data/`.

## Documentação

- [API e contratos das requisições](docs/api.md)
- [Arquitetura, confiabilidade dos dados e limites de desempenho](docs/architecture.md)
- [Comparação com o Milvus e roteiro de desenvolvimento](docs/compatibility.md)
- [Implantação, autenticação, backup e recuperação](docs/operations.md)
- [Registros de validação](docs/validation.md)
- [Site e publicação do domínio](website/README.md)

Os links acima levam à documentação existente; nem todos os documentos estão traduzidos para o português do Brasil.

Diretórios: `crates/engine` — motor do banco de dados; `crates/server` — HTTP / CLI; `docs` — documentação; `examples` — exemplos; `website` — site. O site é publicado separadamente no GitHub Pages em [vectordb.io](https://vectordb.io/).
