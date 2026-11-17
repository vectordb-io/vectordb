use prost::Message;
use serde_json::json;
use vectordb_cluster::contracts::{Envelope, IndexReader, Kind, SegmentReader};
use vectordb_engine::segment::SegmentEngine;
#[test]
fn protobuf_wire_fixture_additive_fields_and_version_rejection() {
    let envelope = Envelope {
        protocol_version: 1,
        collection_generation: "g1".into(),
        kind: Kind::CommitToken as i32,
        body: b"{}".to_vec(),
    };
    let fixture = vec![8, 1, 18, 2, b'g', b'1', 24, 3, 34, 2, b'{', b'}'];
    assert_eq!(envelope.encode_to_vec(), fixture);
    let mut future = fixture;
    future.extend([128, 1, 7]);
    let decoded = Envelope::decode(future.as_slice()).unwrap();
    assert_eq!(decoded, envelope);
    decoded.validate("g1").unwrap();
    assert!(decoded.validate("g2").is_err());
    let mut incompatible = decoded;
    incompatible.protocol_version = 2;
    assert!(incompatible.validate("g1").is_err());
    incompatible.protocol_version = 1;
    incompatible.kind = 999;
    assert!(incompatible.validate("g1").is_err());
    let proto = include_str!("../../../proto/vectordb/internal/v1/core.proto");
    for declaration in [
        "protocol_version = 1",
        "collection_generation = 2",
        "kind = 3",
        "body = 4",
        "reserved 5 to 15",
    ] {
        assert!(proto.contains(declaration));
    }
}
#[test]
fn readers_preserve_version_visibility_and_exact_ranking() {
    let mut engine = SegmentEngine::new();
    let batch = engine
        .prepare(
            "collections/create",
            json!({"collectionName":"items","dimension":2,"metricType":"L2"}),
        )
        .unwrap();
    engine.commit(batch, 1).unwrap();
    let batch=engine.prepare("entities/insert",json!({"collectionName":"items","data":[{"id":1,"vector":[1,0]},{"id":2,"vector":[0,1]}]})).unwrap();
    engine.commit(batch, 4).unwrap();
    let reader: &dyn SegmentReader = &engine;
    assert_eq!(reader.version(), 4);
    assert_eq!(reader.get(&json!(1)).unwrap().version, 4);
    assert!(reader.get(&json!(99)).is_none());
    let index: &mut dyn IndexReader = &mut engine;
    let hits = index
        .search(json!({"collectionName":"items","data":[[1,0]],"limit":2}))
        .unwrap();
    assert_eq!(hits[0]["id"], 1);
    assert_eq!(hits[0]["distance"], 0.0);
}
