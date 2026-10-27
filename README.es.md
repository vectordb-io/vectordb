# VectorDB · Rust

**[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md) | Español | [Português do Brasil](README.pt-BR.md)**

Una base de datos vectorial autónoma reimplementada en Rust, con un motor integrable, un servicio HTTP, persistencia transaccional y un sitio web estático independiente.

**Estado: 0.1.0 Alpha.** Inspirado en el modelo de datos y el estilo de peticiones REST v2 de Milvus, VectorDB implementa un conjunto funcional de capacidades para un solo nodo. La mayor parte de la infraestructura distribuida de Milvus no está implementada, y VectorDB no es un reemplazo directo de Milvus / PyMilvus. Consulta la [comparación de funciones](docs/compatibility.md) para conocer el alcance exacto. Se han eliminado la implementación anterior en C++, su documentación y su sitio web.

## Inicio rápido

Requiere Rust 1.89+, Cargo y herramientas de compilación de C (para compilar la copia de SQLite incluida). En macOS, usa Xcode Command Line Tools; en Linux, el compilador de C del sistema. No necesita un servicio de base de datos externo.

```sh
cargo build --release --locked
./target/release/vectordb serve
```

El servidor escucha en `127.0.0.1:19530` de forma predeterminada y guarda los datos en `data/vectordb.sqlite`. En otra terminal:

```sh
curl http://127.0.0.1:19530/healthz
bash examples/quickstart.sh
```

El ejemplo crea una colección `demo`, inserta dos entidades, busca vectores similares y aplica filtros escalares. Antes de volver a ejecutarlo, elimina `demo` o usa otro nombre de colección.

## Capacidades actuales

- Múltiples bases de datos, colecciones, esquemas explícitos / creación rápida de colecciones, particiones, alias de colecciones, cambio de nombre, carga y liberación.
- Claves primarias Int64 / VarChar e identificadores automáticos; campos escalares, JSON, Array, campos que admiten valores nulos, valores predeterminados y campos dinámicos.
- Inserción por lotes / upsert de entidades completas, query / get / delete, conteo, ordenación y paginación.
- FloatVector, SparseFloatVector, BinaryVector; COSINE, IP, L2 (distancia al cuadrado), HAMMING y JACCARD.
- Búsqueda exacta FLAT, IVF_FLAT, HNSW; prefiltrado escalar, búsqueda por rango, búsqueda agrupada y múltiples vectores de consulta.
- Fusión de búsquedas con múltiples vectores: RRF y clasificación ponderada normalizada.
- SQLite WAL con sincronización FULL; transacciones por lotes, recuperación tras fallos, bloqueo exclusivo del proceso, copias de seguridad mediante instantáneas e importación / exportación JSONL en streaming.
- Tokens Bearer, límites de tamaño de petición y de cola de concurrencia, comprobaciones de estado, contadores de peticiones / errores de Prometheus y apagado ordenado.

Los embeddings no se generan automáticamente; las aplicaciones deben generar y enviar los vectores. La lógica de la base de datos, los filtros, los algoritmos de indexación y el servicio HTTP están escritos en Rust. SQLite, implementado en C, es la dependencia de persistencia integrada.

## Ejemplos de peticiones

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

Respuesta: `{"code":0,"data":[{"id":1,"title":"Rust","distance":1.0}]}`.

## Integración en Rust

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

Ejecuta el ejemplo completo incluido en el repositorio: `cargo run -p vectordb-engine --example embedded`.

## Validación

```sh
cargo fmt --all --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo build --workspace --locked
python3 scripts/smoke.py
python3 scripts/check_website.py
```

Las pruebas cubren la reversión de transacciones, la validación de esquemas, el filtrado, la precisión de las claves primarias, el estado de carga, el aislamiento entre bases de datos / particiones, la actualización de índices, las comparaciones ANN, la búsqueda híbrida, las instantáneas, el bloqueo exclusivo, la corrupción de la base de datos, la autenticación HTTP y las escrituras concurrentes. `smoke.py` inicia un proceso real con una base de datos temporal para verificar que las escrituras confirmadas se pueden recuperar después de SIGKILL. No utiliza los datos reales de `data/`.

## Documentación

- [API y contratos de las peticiones](docs/api.md)
- [Arquitectura, fiabilidad de los datos y límites de rendimiento](docs/architecture.md)
- [Comparación con Milvus y hoja de ruta](docs/compatibility.md)
- [Despliegue, autenticación, copias de seguridad y recuperación](docs/operations.md)
- [Registros de validación](docs/validation.md)
- [Sitio web y publicación del dominio](website/README.md)

Los enlaces anteriores apuntan a la documentación existente; no todos los documentos están traducidos al español.

Directorios: `crates/engine` — motor de base de datos; `crates/server` — HTTP / CLI; `docs` — documentación; `examples` — ejemplos; `website` — sitio web. El sitio se publica por separado en GitHub Pages en [vectordb.io](https://vectordb.io/).
