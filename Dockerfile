FROM rust:1.89-bookworm AS build
WORKDIR /app
COPY Cargo.toml Cargo.lock ./
COPY crates ./crates
RUN cargo build --release --locked -p vectordb-server

FROM debian:bookworm-slim
RUN groupadd --gid 10001 vectordb && useradd --uid 10001 --gid 10001 --no-create-home vectordb \
    && mkdir /data && chown 10001:10001 /data
COPY --from=build /app/target/release/vectordb /usr/local/bin/vectordb
USER 10001:10001
VOLUME ["/data"]
EXPOSE 19530
ENTRYPOINT ["vectordb"]
CMD ["serve", "--bind", "0.0.0.0:19530", "--data", "/data/vectordb.sqlite"]
