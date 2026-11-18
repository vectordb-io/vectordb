use crate::{
    Error, Result,
    state::{Record, Scope},
};
use rdkafka::consumer::{BaseConsumer, Consumer};
use rdkafka::producer::{FutureProducer, FutureRecord, Producer};
use rdkafka::{ClientConfig, Message, Offset, TopicPartitionList};
use std::time::{Duration, Instant};

const TIMEOUT: Duration = Duration::from_secs(15);
// Sixteen maximal public batches bound raw replay payloads to about 8 MiB.
const REPLAY_BATCH: usize = 16;
pub struct Log {
    brokers: String,
    topic: String,
    scope: Scope,
    pub participants: crate::participants::Participants,
    producer: Option<FutureProducer>,
    cursor: std::sync::Mutex<Option<(BaseConsumer, Option<i64>)>>,
}
impl Log {
    pub fn reader(brokers: String, scope: &Scope) -> Self {
        Self {
            scope: scope.clone(),
            participants: crate::participants::Participants::new(brokers.clone()),
            brokers,
            topic: scope.topic(),
            producer: None,
            cursor: std::sync::Mutex::new(None),
        }
    }
    pub fn remove_closed_topic(&self) -> Result<()> {
        use rdkafka::admin::{AdminClient, AdminOptions};
        let admin: AdminClient<_> = ClientConfig::new()
            .set("bootstrap.servers", &self.brokers)
            .create::<AdminClient<rdkafka::client::DefaultClientContext>>()?;
        for res in futures::executor::block_on(admin.delete_topics(
            &[&self.topic, &format!("{}.prepare", self.topic)],
            &AdminOptions::new().operation_timeout(Some(TIMEOUT)),
        ))? {
            if let Err((_, e)) = res
                && e != rdkafka::error::RDKafkaErrorCode::UnknownTopicOrPartition
            {
                return Err(Error::Unavailable(format!("remove retired WAL: {e}")));
            }
        }
        Ok(())
    }
    pub fn watermarks(&self) -> Result<(i64, i64)> {
        let consumer: BaseConsumer = ClientConfig::new()
            .set("bootstrap.servers", &self.brokers)
            .set("group.id", "vdb-storage-stats")
            .set("enable.auto.commit", "false")
            .create()?;
        Ok(consumer.fetch_watermarks(&self.topic, 0, TIMEOUT)?)
    }
    pub fn ensure_topic(&self, replicas: i32) -> Result<()> {
        use rdkafka::admin::{AdminClient, AdminOptions, NewTopic, TopicReplication};
        let admin: AdminClient<_> = ClientConfig::new()
            .set("bootstrap.servers", &self.brokers)
            .create::<AdminClient<rdkafka::client::DefaultClientContext>>()?;
        let topic = NewTopic::new(&self.topic, 1, TopicReplication::Fixed(replicas))
            .set("retention.ms", "-1")
            .set("retention.bytes", "-1")
            .set("cleanup.policy", "delete")
            .set("max.message.bytes", "2097152");
        for result in futures::executor::block_on(admin.create_topics(
            &[topic],
            &AdminOptions::new().operation_timeout(Some(TIMEOUT)),
        ))? {
            if let Err((_, code)) = result
                && code != rdkafka::error::RDKafkaErrorCode::TopicAlreadyExists
            {
                return Err(Error::Unavailable(format!("create WAL topic: {code}")));
            }
        }
        Ok(())
    }
    /// Call only after acquiring a new catalog ownership lease. Never retry on
    /// the same owner after fencing or an uncertain transaction outcome.
    pub fn initialize_writer(&mut self) -> Result<()> {
        let producer: FutureProducer = ClientConfig::new()
            .set("bootstrap.servers", &self.brokers)
            .set("transactional.id", &self.topic)
            .set("enable.idempotence", "true")
            .set("acks", "all")
            .set("linger.ms", "0")
            .set("message.timeout.ms", "10000")
            .set("transaction.timeout.ms", "15000")
            .set("message.max.bytes", "2097152")
            .create()?;
        producer.init_transactions(TIMEOUT)?;
        self.producer = Some(producer);
        Ok(())
    }
    pub fn append(&self, record: &Record) -> Result<i64> {
        self.append_bytes(serde_json::to_vec(record)?)
    }
    pub fn decide(&self, decision: &crate::channels::Decision) -> Result<i64> {
        self.append_bytes(serde_json::to_vec(decision)?)
    }
    fn append_bytes(&self, bytes: Vec<u8>) -> Result<i64> {
        let producer = self
            .producer
            .as_ref()
            .ok_or_else(|| Error::Unavailable("read-only log".into()))?;
        if bytes.len() > 1024 * 1024 {
            return Err(Error::Invalid("WAL entry exceeds 1 MiB".into()));
        }
        producer.begin_transaction()?;
        let future = producer
            .send_result(
                FutureRecord::to(&self.topic)
                    .partition(0)
                    .key("collection")
                    .payload(&bytes),
            )
            .map_err(|(error, _)| Error::Kafka(error))?;
        let delivery = futures::executor::block_on(future)
            .map_err(|_| Error::Unavailable("delivery cancelled; outcome unknown".into()))?
            .map_err(|(error, _)| Error::Kafka(error))?;
        producer.commit_transaction(TIMEOUT)?;
        Ok(delivery.offset)
    }
    /// Call only after an immutable, verified state checkpoint is published.
    pub fn trim(&self, through: i64) -> Result<()> {
        use rdkafka::admin::{AdminClient, AdminOptions};
        let admin: AdminClient<_> = ClientConfig::new()
            .set("bootstrap.servers", &self.brokers)
            .create::<AdminClient<rdkafka::client::DefaultClientContext>>()?;
        let mut offsets = TopicPartitionList::new();
        offsets.add_partition_offset(&self.topic, 0, Offset::Offset(through + 1))?;
        let result = futures::executor::block_on(admin.delete_records(
            &offsets,
            &AdminOptions::new().operation_timeout(Some(TIMEOUT)),
        ))?;
        for element in result.elements() {
            element.error()?;
        }
        Ok(())
    }
    pub fn replay(
        &self,
        after: i64,
        through: i64,
        mut apply: impl FnMut(i64, Record) -> Result<()>,
    ) -> Result<()> {
        self.replay_range(after, Some(through), &mut apply)
    }
    /// Used for a terminal (dropped) generation whose mutations have stopped.
    pub fn replay_tail(
        &self,
        after: i64,
        apply: impl FnMut(i64, Record) -> Result<()>,
    ) -> Result<()> {
        self.replay_range(after, None, apply)
    }
    fn replay_range(
        &self,
        after: i64,
        through: Option<i64>,
        mut apply: impl FnMut(i64, Record) -> Result<()>,
    ) -> Result<()> {
        if through.is_some_and(|n| n < after) {
            return Err(Error::Conflict("snapshot is older than replica".into()));
        }
        if through == Some(after) {
            return Ok(());
        }
        let mut cursor = self.cursor.lock().unwrap();
        if cursor.is_none() {
            let consumer: BaseConsumer = ClientConfig::new()
                .set("bootstrap.servers", &self.brokers)
                .set("group.id", format!("vdb-reader-{}", uuid::Uuid::new_v4()))
                .set("enable.auto.commit", "false")
                .set("enable.auto.offset.store", "false")
                .set("enable.partition.eof", "true")
                .set("isolation.level", "read_committed")
                .set("fetch.wait.max.ms", "10")
                .set("fetch.queue.backoff.ms", "5")
                .set("queued.max.messages.kbytes", "2048")
                .set("auto.offset.reset", "error")
                .create()?;
            *cursor = Some((consumer, None));
        }
        let (consumer, last) = cursor.as_mut().unwrap();
        // On a failed callback or a rewind, reassign from the caller's durable position.
        let consecutive = last.take() == Some(after);
        let metadata = consumer.fetch_metadata(Some(&self.topic), TIMEOUT)?;
        let topic = metadata
            .topics()
            .iter()
            .find(|t| t.name() == self.topic)
            .ok_or_else(|| Error::Unavailable("collection topic missing".into()))?;
        if topic.partitions().len() != 1 {
            return Err(Error::Integrity(
                "collection WAL must have exactly one partition".into(),
            ));
        }
        let (low, high) = consumer.fetch_watermarks(&self.topic, 0, TIMEOUT)?;
        if after + 1 < low {
            return Err(Error::CheckpointRequired);
        }
        if through.is_none() && after + 1 >= high {
            return Ok(());
        }
        if !consecutive {
            let mut assignment = TopicPartitionList::new();
            assignment.add_partition_offset(&self.topic, 0, Offset::Offset(after + 1))?;
            consumer.assign(&assignment)?;
        }
        let mut deadline = Instant::now() + TIMEOUT;
        let mut pending = Vec::new();
        let resolve = |pending: &mut Vec<(i64, crate::channels::Decision)>,
                       apply: &mut dyn FnMut(i64, Record) -> Result<()>|
         -> Result<()> {
            if !pending.is_empty() {
                let requests = self.participants.read_batch(&self.scope, pending)?;
                for ((position, d), r) in pending.drain(..).zip(requests) {
                    apply(position, d.resolve(r)?)?;
                }
            }
            Ok(())
        };

        while Instant::now() < deadline {
            if let Some(message) = consumer.poll(Duration::from_millis(100)) {
                if matches!(message, Err(rdkafka::error::KafkaError::PartitionEOF(_))) {
                    if through.is_none() {
                        let position = consumer.position()?;
                        if position
                            .find_partition(&self.topic, 0)
                            .is_some_and(|p| matches!(p.offset(),Offset::Offset(o) if o>=high))
                        {
                            return resolve(&mut pending, &mut apply);
                        }
                    }
                    continue;
                }
                let message = message?;
                let offset = message.offset();
                if through.is_some_and(|n| offset > n) {
                    return Err(Error::Integrity("read barrier record is absent".into()));
                }
                let bytes = message
                    .payload()
                    .ok_or_else(|| Error::Integrity("empty WAL record".into()))?;
                if bytes.len() > 1024 * 1024 {
                    return Err(Error::Integrity("oversized WAL entry".into()));
                }
                // Legacy records and control operations keep their original wire format.
                match serde_json::from_slice::<Record>(bytes) {
                    Ok(r) => {
                        resolve(&mut pending, &mut apply)?;
                        apply(offset, r)?;
                    }
                    Err(_) => {
                        let d: crate::channels::Decision = serde_json::from_slice(bytes)?;
                        if d.format != 3 || d.scope != self.scope {
                            return Err(Error::Integrity("foreign or unsupported decision".into()));
                        }
                        pending.push((offset, d));
                        if pending.len() >= REPLAY_BATCH {
                            resolve(&mut pending, &mut apply)?;
                        }
                    }
                };
                deadline = Instant::now() + TIMEOUT;
                if Some(offset) == through {
                    resolve(&mut pending, &mut apply)?;
                    *last = Some(offset);
                    return Ok(());
                }
            }
        }
        Err(Error::Unavailable(
            "timed out waiting for committed WAL prefix".into(),
        ))
    }
}
