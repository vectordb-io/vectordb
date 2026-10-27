# VectorDB · Rust

**[English](README.md) | [简体中文](README.zh-CN.md) | 日本語 | [Español](README.es.md) | [Português do Brasil](README.pt-BR.md)**

Rust で再実装した単一ノードのベクトルデータベースです。組み込み可能なエンジン、HTTP サービス、トランザクションによる永続化、独立した静的な公式サイトを提供します。

**ステータス：0.1.0 Alpha。** Milvus のデータモデルと REST v2 のリクエスト形式を参考に、単一ノードで利用できる一連の機能を実装しています。Milvus の分散インフラの大部分は未実装であり、Milvus / PyMilvus をそのまま置き換えるものではありません。対応範囲の詳細は[機能比較](docs/compatibility.md)をご覧ください。旧 C++ 実装、旧ドキュメント、旧公式サイトは削除済みです。

## クイックスタート

Rust 1.89 以降、Cargo、C コンパイラのツールチェーン（同梱の SQLite のコンパイル用）が必要です。macOS では Xcode Command Line Tools、Linux ではシステムの C コンパイラを使用します。外部のデータベースサービスは不要です。

```sh
cargo build --release --locked
./target/release/vectordb serve
```

デフォルトでは `127.0.0.1:19530` で待ち受け、データを `data/vectordb.sqlite` に保存します。別のターミナルで次を実行します。

```sh
curl http://127.0.0.1:19530/healthz
bash examples/quickstart.sh
```

このサンプルは `demo` コレクションを作成し、2 件のエンティティを挿入して、類似ベクトルの検索とスカラーフィルタリングを実行します。再実行する前に `demo` を削除するか、別のコレクション名を使用してください。

## 現在の機能

- 複数のデータベース、コレクション、明示的なスキーマ定義 / 簡易コレクション作成、パーティション、コレクションのエイリアス、名前変更、ロードと解放。
- Int64 / VarChar 主キー、自動 ID、スカラーフィールド、JSON、Array、NULL 許容フィールド、デフォルト値、動的フィールド。
- 一括 insert / エンティティ全体の upsert、query / get / delete、件数取得、ソート、ページネーション。
- FloatVector、SparseFloatVector、BinaryVector、COSINE、IP、L2（二乗距離）、HAMMING、JACCARD。
- FLAT による厳密検索、IVF_FLAT、HNSW、スカラー条件による事前フィルタリング、範囲検索、グループ化検索、複数のクエリベクトル。
- 複数ベクトルの検索結果の統合：RRF と正規化された重み付きランキング。
- SQLite WAL と FULL 同期レベル、一括トランザクション、クラッシュリカバリ、プロセスの排他ロック、スナップショットバックアップ、JSONL のストリーミングインポート / エクスポート。
- Bearer トークン、リクエストサイズと同時実行キューの制限、ヘルスチェック、Prometheus のリクエスト / エラーカウンター、グレースフルシャットダウン。

埋め込みベクトル（embedding）の自動生成機能はありません。ベクトルはアプリケーション側で生成して送信します。独自のデータベースロジック、フィルター、インデックスアルゴリズム、HTTP サービスは Rust で実装されています。永続化には、C で実装された組み込みの SQLite を使用します。

## リクエスト例

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

レスポンス：`{"code":0,"data":[{"id":1,"title":"Rust","distance":1.0}]}`。

## Rust アプリケーションへの組み込み

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

リポジトリに含まれる完全なサンプルを実行するには、`cargo run -p vectordb-engine --example embedded` を使用します。

## 検証

```sh
cargo fmt --all --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo build --workspace --locked
python3 scripts/smoke.py
python3 scripts/check_website.py
```

テストは、トランザクションのロールバック、スキーマ検証、フィルタリング、主キーの精度、ロード状態、データベース / パーティションの分離、インデックス更新、ANN の比較、ハイブリッド検索、スナップショット、排他ロック、データベース破損、HTTP 認証、並行書き込みを対象としています。`smoke.py` は一時データベースを使って実際のプロセスを起動し、SIGKILL 後に応答済みの書き込みを復元できることを検証します。`data/` 内の実データは使用しません。

## ドキュメント

- [API とリクエスト仕様](docs/api.md)
- [アーキテクチャ、データの信頼性、性能上の制約](docs/architecture.md)
- [Milvus との機能比較とロードマップ](docs/compatibility.md)
- [デプロイ、認証、バックアップと復元](docs/operations.md)
- [検証記録](docs/validation.md)
- [公式サイトとドメインの公開](website/README.md)

ディレクトリ構成：`crates/engine` はデータベースエンジン、`crates/server` は HTTP / CLI、`docs` はドキュメント、`examples` はサンプル、`website` は公式サイトです。公式サイトは GitHub Pages に独立してデプロイされ、[vectordb.io](https://vectordb.io/) で公開されています。
