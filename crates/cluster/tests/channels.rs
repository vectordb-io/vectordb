use serde_json::{Value, json};
use std::collections::BTreeMap;
use vectordb_cluster::{
    channels::{Part, Prepared, Vote},
    decisions::WriteHead,
    digest,
    state::{Entry, FORMAT, Record, Replica, Scope, idempotency_key},
};
fn scope() -> Scope {
    Scope {
        cluster: "model".into(),
        collection: "items".into(),
        generation: "g".into(),
    }
}
fn initial(auto: bool) -> Replica {
    let mut r = Replica::empty(scope()).unwrap();
    r.apply(
        0,
        Record {
            scope: scope(),
            format: FORMAT,
            entry: Entry::Activate { epoch: 1 },
        },
    )
    .unwrap();
    let create = r
        .prepare_at(
            1,
            &idempotency_key(100, "create"),
            "collections/create",
            json!({"collectionName":"items","dimension":2,"autoID":auto}),
            100,
        )
        .unwrap()
        .unwrap();
    r.apply(1, create).unwrap();
    r
}
fn pieces(op: &str, key: &str, body: &Value, offset: i64) -> (Prepared, BTreeMap<i32, Vec<u8>>) {
    let (mut p, fragments) = Prepared::split(&scope(), 4, "id", key, op, body.clone()).unwrap();
    p.parts = fragments
        .iter()
        .map(|(c, v)| Part {
            channel: *c,
            offset,
            sha256: digest(v),
        })
        .collect();
    (p, fragments)
}
fn vote(r: &Replica, op: &str, key: &str, body: &Value, offset: i64) -> Vote {
    let (p, _) = pieces(op, key, body, offset);
    Vote::sign(
        &scope(),
        "test",
        p,
        r.spec().unwrap().write_intents(op, body).unwrap(),
        serde_json::to_vec(body).unwrap().len(),
    )
    .unwrap()
}
#[test]
fn verified_prepare_requires_exact_order_identity_and_all_channels() {
    let body = json!({"collectionName":"items","data":[{"id":"z"},{"id":1},{"id":"1"},{"id":3}]});
    let (p, parts) = pieces("entities/insert", "v2-100-k", &body, 7);
    assert!(parts.len() >= 2);
    assert_eq!(p.assemble(&scope(), parts.clone()).unwrap(), body);
    let mut absent = parts.clone();
    absent.pop_first();
    assert!(p.assemble(&scope(), absent).is_err());
    let mut bad = parts.clone();
    bad.first_entry().unwrap().get_mut().push(0);
    assert!(p.assemble(&scope(), bad).is_err());
    let mut forged = p.clone();
    forged.parts.reverse();
    assert!(forged.assemble(&scope(), parts.clone()).is_err());
    let mut other = scope();
    other.generation = "other".into();
    assert!(p.assemble(&other, parts.clone()).is_err());
    let mut other = p.clone();
    other.request_id = "v2-100-other".into();
    assert!(other.assemble(&scope(), parts).is_err());
}
#[test]
fn compact_decisions_match_independent_full_engine_and_survive_every_cut() {
    for auto in [false, true] {
        let mut materialized = initial(auto);
        let mut reference = initial(auto);
        let mut head = WriteHead::default();
        for i in 0..120 {
            let id = (i * 13) % 17;
            let op = [
                "entities/insert",
                "entities/upsert",
                "entities/delete",
                "entities/upsert",
            ][i % 4];
            let key = idempotency_key(100, &format!("k{i}"));
            let mut row = json!({"id":id,"vector":[0.000001,i],"value":i%7});
            if auto && op == "entities/insert" {
                row.as_object_mut().unwrap().remove("id");
            }
            let body = if op == "entities/delete" {
                json!({"collectionName":"items","ids":[id]})
            } else {
                json!({"collectionName":"items","data":[row]})
            };
            let expected = reference.prepare_at(1, &key, op, body.clone(), 100);
            let v = vote(&materialized, op, &key, &body, i as i64);
            v.verify(&scope(), 4, "test").unwrap();
            let actual = head.prepare(&materialized, v, 256 * 1024 * 1024, 100);
            if expected.is_err() {
                assert!(actual.is_err());
                continue;
            }
            let record = expected.unwrap().unwrap();
            let plan = actual.unwrap().unwrap();
            let offset = i as i64 + 2;
            assert_eq!(
                plan.decision.result,
                match &record.entry {
                    Entry::Mutation { result, .. } => result.clone(),
                    _ => unreachable!(),
                }
            );
            let result = head.committed(plan, offset).unwrap();
            reference.apply(offset, record).unwrap();
            let retry = head
                .prepare(
                    &materialized,
                    vote(&materialized, op, &key, &body, offset + 1000),
                    256 * 1024 * 1024,
                    100,
                )
                .unwrap()
                .err()
                .unwrap();
            assert_eq!(retry.token, result.token);
            assert_eq!(
                head.bytes().unwrap(),
                reference.state_bytes(),
                "auto={auto} op={op} step={i}"
            );
            if i % 7 == 0 {
                let cp = reference.checkpoint().unwrap();
                materialized = Replica::restore(cp.checkpoint, &scope(), |k| {
                    Ok(cp
                        .sealed
                        .objects
                        .iter()
                        .find(|(name, _)| name == k)
                        .unwrap()
                        .1
                        .clone())
                })
                .unwrap();
                head = WriteHead::default();
            }
        }
    }
}
#[test]
fn old_prepares_cannot_cross_cut_and_votes_bind_scope_and_cost() {
    let mut r = initial(false);
    r.apply(
        4,
        Record {
            format: FORMAT,
            scope: scope(),
            entry: Entry::ChannelCut {
                epoch: 1,
                floors: vec![8; 4],
            },
        },
    )
    .unwrap();
    let cp = r.checkpoint().unwrap();
    let r = Replica::restore(cp.checkpoint, &scope(), |k| {
        Ok(cp
            .sealed
            .objects
            .iter()
            .find(|(name, _)| name == k)
            .unwrap()
            .1
            .clone())
    })
    .unwrap();
    let b = json!({"collectionName":"items","data":[{"id":1,"vector":[1,0]}]});
    let key = "v2-100-one";
    let op = "entities/insert";
    let mut v = vote(&r, op, key, &b, 8);
    assert!(v.verify(&scope(), 4, "other").is_err());
    v.request_bytes += 1;
    assert!(v.verify(&scope(), 4, "test").is_err());
    let head = WriteHead::default();
    assert!(
        head.prepare(&r, vote(&r, op, key, &b, 7), 1 << 28, 100)
            .is_err()
    );
    assert!(
        head.prepare(&r, vote(&r, op, key, &b, 8), 1 << 28, 100)
            .unwrap()
            .is_ok()
    );
    assert!(head.prepare(&r, vote(&r, op, key, &b, 8), 1, 100).is_err());
    assert!(vectordb_cluster::channels::validate_cut(&[8; 4], &[7; 4]).is_err());
}

#[test]
fn every_interleaving_of_two_channel_prepares_and_conflicting_decisions() {
    // Enumerate the 20 merges of (prepare left, prepare right, decide) for two
    // competing inserts. Restart the reader at every prefix, using no writer state.
    for mask in 0u32..64 {
        if mask.count_ones() != 3 {
            continue;
        }
        let replica = initial(false);
        let mut head = WriteHead::default();
        let mut next = [0usize; 2];
        let mut prepared = [BTreeMap::new(), BTreeMap::new()];
        let mut journal = vec![];
        let mut winner = None;
        for step in 0..6 {
            let tx = ((mask >> step) & 1) as usize;
            let body = json!({"collectionName":"items","data":[{"id":1,"vector":[tx,0]},{"id":2,"vector":[tx,1]}]});
            let id = idempotency_key(100, &format!("tx{tx}"));
            let (p, parts) = pieces("entities/insert", &id, &body, tx as i64);
            assert_eq!(parts.len(), 2);
            if next[tx] < 2 {
                let (channel, bytes) = parts.iter().nth(next[tx]).unwrap();
                prepared[tx].insert(*channel, bytes.clone());
            } else {
                let v = vote(&replica, "entities/insert", &id, &body, tx as i64);
                let r = head.prepare(&replica, v, 1 << 28, 100);
                if winner.is_none() {
                    let plan = r.unwrap().unwrap();
                    journal.push((step + 2, serde_json::to_vec(&plan.decision).unwrap()));
                    head.committed(plan, step + 2).unwrap();
                    winner = Some(tx);
                } else {
                    assert!(r.is_err());
                }
            }
            next[tx] += 1;
            let mut reader = initial(false);
            for (n, bytes) in &journal {
                let decision: vectordb_cluster::channels::Decision =
                    serde_json::from_slice(bytes).unwrap();
                let tx = if decision.prepared.request_id.ends_with("tx0") {
                    0
                } else {
                    1
                };
                let request = decision
                    .prepared
                    .assemble(&scope(), prepared[tx].clone())
                    .unwrap();
                reader
                    .apply(*n, decision.resolve(request).unwrap())
                    .unwrap();
            }
            let snapshot = reader.logical_backup();
            assert_eq!(snapshot.rows.len(), if winner.is_some() { 2 } else { 0 });
            for row in snapshot.rows {
                assert_eq!(
                    row.body.unwrap()["vector"][0].as_f64(),
                    winner.map(|i| i as f64)
                );
            }
            if next[tx] < 2 {
                assert!(p.assemble(&scope(), prepared[tx].clone()).is_err());
            }
        }
    }
}
