use serde_json::json;
use std::collections::{BTreeMap, BTreeSet};
use vectordb_cluster::{
    control::{ControlState, Member, NodeRole, plan},
    directory::{Collection, Lifecycle},
    state::Scope,
};
fn member(id: &str, zone: &str, role: NodeRole) -> Member {
    Member {
        id: id.into(),
        incarnation: format!("boot-{id}"),
        zone: zone.into(),
        role,
        origin: format!("http://{id}:8000"),
        rpc: format!("https://{id}:9000"),
        protocol_min: 1,
        protocol_max: 1,
        max_shards: 128,
        max_collections: 16,
        resources: json!({}),
    }
}
fn collection(id: &str) -> Collection {
    Collection {
        database: "default".into(),
        name: id.into(),
        scope: Scope {
            cluster: "test".into(),
            collection: id.into(),
            generation: "g1".into(),
        },
        state: Lifecycle::Active,
        create_request: json!({}),
        create_id: "create".into(),
        drop_id: None,
        dropped_at_ms: None,
        spec: None,
        spec_offset: 0,
        shard_count: 16,
        write_channels: None,
        query_routes: None,
    }
}
#[test]
fn placement_is_deterministic_balanced_and_spreads_failure_domains() {
    let members = [
        member("w1", "a", NodeRole::Writer),
        member("w2", "b", NodeRole::Writer),
        member("q1", "a", NodeRole::Query),
        member("q2", "b", NodeRole::Query),
        member("q3", "c", NodeRole::Query),
    ]
    .into_iter()
    .map(|m| (m.id.clone(), m))
    .collect::<BTreeMap<_, _>>();
    let collections = (0..4)
        .map(|n| {
            let id = format!("c{n}");
            (id.clone(), collection(&id))
        })
        .collect();
    let state = ControlState::default();
    let result = plan(&collections, &members, &state);
    assert_eq!(result, plan(&collections, &members, &state));
    let mut loads = BTreeMap::<String, usize>::new();
    for p in result.values() {
        for r in &p.replicas {
            assert_eq!(r.len(), 2);
            assert_ne!(members[&r[0]].zone, members[&r[1]].zone);
            for id in r {
                *loads.entry(id.clone()).or_default() += 1;
            }
        }
    }
    assert!(loads.values().max().unwrap() - loads.values().min().unwrap() <= 1);
    let mut drained = state;
    drained.draining.insert("q1".into());
    let next = plan(&collections, &members, &drained);
    assert!(
        next.values()
            .all(|p| p.replicas.iter().flatten().all(|id| id != "q1"))
    );
}
#[test]
fn capacities_incompatible_versions_and_lost_domains_fail_closed() {
    let mut bad = member("bad", "a", NodeRole::Query);
    bad.protocol_min = 2;
    bad.protocol_max = 2;
    assert!(bad.validate().is_err());
    bad.protocol_min = 1;
    bad.rpc = "http://bad:9000".into();
    assert!(bad.validate().is_err());
    let mut query = member("q1", "a", NodeRole::Query);
    query.max_shards = 8;
    let members = [
        member("w1", "a", NodeRole::Writer),
        query,
        member("q2", "a", NodeRole::Query),
    ]
    .into_iter()
    .map(|m| (m.id.clone(), m))
    .collect();
    let collections = BTreeMap::from([("c1".into(), collection("c1"))]);
    let result = plan(&collections, &members, &ControlState::default());
    assert!(result["c1"].replicas.iter().all(|r| r.len() == 1)); // same zone cannot pretend to be two domains
    assert!(
        result["c1"]
            .replicas
            .iter()
            .flatten()
            .filter(|id| id.as_str() == "q1")
            .count()
            <= 8
    );
    let state = ControlState {
        draining: BTreeSet::from(["w1".into()]),
        ..ControlState::default()
    };
    assert!(plan(&collections, &members, &state).is_empty());
}

#[test]
fn query_collection_capacity_is_respected_across_assignments() {
    let w = member("w1", "a", NodeRole::Writer);
    let mut q = member("q1", "b", NodeRole::Query);
    q.max_collections = 1;
    let members = BTreeMap::from([("w1".into(), w), ("q1".into(), q)]);
    let collections = BTreeMap::from([
        ("c1".into(), collection("c1")),
        ("c2".into(), collection("c2")),
    ]);
    let planned = plan(&collections, &members, &ControlState::default());
    assert!(planned["c1"].replicas.iter().all(|r| r.len() == 1));
    assert!(planned["c2"].replicas.iter().all(Vec::is_empty));
}
