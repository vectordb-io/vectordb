FROM rust:1.89-bookworm AS build
RUN apt-get update && apt-get install -y --no-install-recommends cmake && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY Cargo.toml Cargo.lock ./
COPY crates crates
COPY proto proto
RUN cargo build --release --locked -p vectordb-cluster --bins

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/target/release/vectordb-cluster /usr/local/bin/
COPY --from=build /src/target/release/vectordb-p1-export /usr/local/bin/
USER 10001:10001
ENTRYPOINT ["vectordb-cluster"]
