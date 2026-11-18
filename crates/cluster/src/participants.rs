//! Kafka participant partitions. Prepares alone have no effect on a collection.
use crate::{
    Error, Result,
    channels::{Part, Prepared},
    digest,
    state::Scope,
};
use rdkafka::{
    ClientConfig, Message, Offset, TopicPartitionList,
    admin::{AdminClient, AdminOptions, NewTopic, TopicReplication},
    client::DefaultClientContext,
    consumer::{BaseConsumer, Consumer},
    producer::{FutureProducer, FutureRecord},
};
use std::{
    collections::BTreeMap,
    sync::{Mutex, OnceLock},
    time::{Duration, Instant},
};
const TIMEOUT: Duration = Duration::from_secs(15);

pub struct Participants {
    brokers: String,
    producer: OnceLock<FutureProducer>,
    readers: Mutex<Vec<BaseConsumer>>,
}
fn topic(scope: &Scope) -> String {
    format!("{}.prepare", scope.topic())
}
impl Participants {
    pub fn new(brokers: String) -> Self {
        Self {
            brokers,
            producer: OnceLock::new(),
            readers: Mutex::new(vec![]),
        }
    }
    fn config(&self) -> ClientConfig {
        let mut config = ClientConfig::new();
        config
            .set("bootstrap.servers", &self.brokers)
            .set("allow.auto.create.topics", "false");
        config
    }
    fn admin(&self) -> Result<AdminClient<DefaultClientContext>> {
        Ok(self.config().create()?)
    }
    fn consumer(&self) -> Result<BaseConsumer> {
        Ok(self
            .config()
            .set("group.id", "vectordb-prepare-reader")
            .set("enable.auto.commit", "false")
            .set("enable.auto.offset.store", "false")
            .set("auto.offset.reset", "error")
            .set("fetch.wait.max.ms", "5")
            .set("fetch.queue.backoff.ms", "2")
            .set("queued.max.messages.kbytes", "2048")
            .create()?)
    }
    pub fn ensure(&self, scope: &Scope, channels: usize, replicas: i32) -> Result<()> {
        crate::channels::validate_count(channels)?;
        let name = topic(scope);
        let admin = self.admin()?;
        let spec = NewTopic::new(&name, channels as i32, TopicReplication::Fixed(replicas))
            .set("retention.ms", "-1")
            .set("retention.bytes", "-1")
            .set("cleanup.policy", "delete")
            .set("max.message.bytes", "2097152");
        for r in futures::executor::block_on(admin.create_topics(
            &[spec],
            &AdminOptions::new().operation_timeout(Some(TIMEOUT)),
        ))? {
            if let Err((_, e)) = r
                && e != rdkafka::error::RDKafkaErrorCode::TopicAlreadyExists
            {
                return Err(Error::Unavailable(format!("prepare topic: {e}")));
            }
        }
        let consumer = self.consumer()?;
        let deadline = Instant::now() + TIMEOUT;
        while let Some(remaining) = deadline.checked_duration_since(Instant::now()) {
            let metadata = consumer.fetch_metadata(Some(&name), remaining)?;
            if let Some(t) = metadata.topics().iter().find(|t| t.name() == name)
                && t.error().is_none()
                && !t.partitions().is_empty()
            {
                if t.partitions().len() != channels {
                    return Err(Error::Integrity("participant layout mismatch".into()));
                }
                if t.partitions()
                    .iter()
                    .all(|p| p.leader() >= 0 && p.error().is_none())
                {
                    return Ok(());
                }
            }
            // Kafka can acknowledge creation before a metadata request sees the leaders.
            std::thread::sleep(Duration::from_millis(50));
        }
        Err(Error::Unavailable(
            "prepare partition leaders are not ready".into(),
        ))
    }
    pub fn stage(
        &self,
        scope: &Scope,
        mut prepared: Prepared,
        fragments: BTreeMap<i32, Vec<u8>>,
    ) -> Result<Prepared> {
        if self.producer.get().is_none() {
            let p = self
                .config()
                .set("enable.idempotence", "true")
                .set("acks", "all")
                .set("linger.ms", "0")
                .set("message.timeout.ms", "12000")
                .set("message.max.bytes", "2097152")
                .create()?;
            let _ = self.producer.set(p);
        }
        let producer = self.producer.get().unwrap();
        let name = topic(scope);
        let mut pending = vec![];
        for (channel, bytes) in fragments {
            let sent = producer
                .send_result(
                    FutureRecord::to(&name)
                        .partition(channel)
                        .key(&prepared.request_id)
                        .payload(&bytes),
                )
                .map_err(|(e, _)| e)?;
            pending.push((channel, digest(&bytes), sent));
            #[cfg(debug_assertions)]
            if std::env::var("VECTORDB_TEST_CRASH_AT").as_deref() == Ok("channel_first_prepare") {
                let (_, _, ack) = pending.pop().unwrap();
                futures::executor::block_on(ack)
                    .map_err(|_| Error::Unavailable("prepare cancelled".into()))?
                    .map_err(|(e, _)| e)?;
                std::process::abort();
            }
        }
        for (channel, sha256, sent) in pending {
            let ack = futures::executor::block_on(sent)
                .map_err(|_| Error::Unavailable("prepare cancelled".into()))?
                .map_err(|(e, _)| e)?;
            prepared.parts.push(Part {
                channel,
                offset: ack.offset,
                sha256,
            });
        }
        Ok(prepared)
    }
    /// A checkpoint or a recovering reader commonly needs many adjacent decisions.
    /// Fetch each partition once, then assemble each batch in decision order.
    pub(crate) fn read_batch(
        &self,
        scope: &Scope,
        batch: &[(i64, crate::channels::Decision)],
    ) -> Result<Vec<serde_json::Value>> {
        use std::collections::BTreeSet;
        let mut wanted = BTreeMap::<i32, BTreeSet<i64>>::new();
        let mut total = 0;
        for (_, d) in batch {
            d.prepared.validate(scope, crate::channels::MAX_CHANNELS)?;
            for part in &d.prepared.parts {
                if !wanted.entry(part.channel).or_default().insert(part.offset) {
                    return Err(Error::Integrity(
                        "reused prepare in committed decisions".into(),
                    ));
                }
                total += 1;
            }
        }
        let reader = self.readers.lock().unwrap().pop();
        let reader = match reader {
            Some(r) => r,
            None => self.consumer()?,
        };
        let mut offsets = TopicPartitionList::new();
        let name = topic(scope);
        for (&channel, positions) in &wanted {
            offsets.add_partition_offset(
                &name,
                channel,
                Offset::Offset(*positions.first().unwrap()),
            )?;
        }
        reader.assign(&offsets)?;
        let deadline = Instant::now() + TIMEOUT;
        let mut payloads = BTreeMap::new();
        let mut bytes = 0usize;
        while payloads.len() < total {
            if Instant::now() >= deadline {
                return Err(Error::Unavailable("timed out fetching all prepares".into()));
            }
            let Some(message) = reader.poll(Duration::from_millis(50)) else {
                continue;
            };
            let msg = match message {
                Ok(m) => m,
                Err(rdkafka::error::KafkaError::MessageConsumption(
                    rdkafka::error::RDKafkaErrorCode::OffsetOutOfRange
                    | rdkafka::error::RDKafkaErrorCode::AutoOffsetReset,
                )) => return Err(Error::CheckpointRequired),
                Err(e) => return Err(e.into()),
            };
            let Some(positions) = wanted.get_mut(&msg.partition()) else {
                return Err(Error::Integrity("unassigned participant".into()));
            };
            let Some(&next) = positions.first() else {
                continue;
            };
            if msg.offset() < next {
                continue;
            } // An orphan or a prepare for another decision.
            if msg.offset() > next {
                return Err(Error::CheckpointRequired);
            }
            let payload = msg
                .payload()
                .ok_or_else(|| Error::Integrity("empty participant".into()))?;
            bytes += payload.len();
            if bytes > batch.len() * 2 * 1024 * 1024 {
                return Err(Error::Integrity("oversized participant batch".into()));
            }
            payloads.insert((msg.partition(), next), payload.to_vec());
            positions.pop_first();
        }
        if reader.unassign().is_ok() {
            let mut pool = self.readers.lock().unwrap();
            if pool.len() < 8 {
                pool.push(reader);
            }
        }
        batch
            .iter()
            .map(|(_, d)| {
                let mut fragments = BTreeMap::new();
                for part in &d.prepared.parts {
                    let data = payloads
                        .remove(&(part.channel, part.offset))
                        .ok_or_else(|| Error::Integrity("missing participant".into()))?;
                    fragments.insert(part.channel, data);
                }
                d.prepared.assemble(scope, fragments)
            })
            .collect()
    }
    pub fn watermarks(&self, scope: &Scope, channels: usize) -> Result<Vec<(i64, i64)>> {
        let c = self.consumer()?;
        let name = topic(scope);
        (0..channels)
            .map(|i| Ok(c.fetch_watermarks(&name, i as i32, TIMEOUT)?))
            .collect()
    }
    pub fn trim(&self, scope: &Scope, floors: &[i64]) -> Result<()> {
        let mut offsets = TopicPartitionList::new();
        for (i, n) in floors.iter().enumerate() {
            if *n > 0 {
                offsets.add_partition_offset(&topic(scope), i as i32, Offset::Offset(*n))?;
            }
        }
        if offsets.count() > 0 {
            let result = futures::executor::block_on(self.admin()?.delete_records(
                &offsets,
                &AdminOptions::new().operation_timeout(Some(TIMEOUT)),
            ))?;
            for e in result.elements() {
                e.error()?;
            }
        }
        Ok(())
    }
}
