use crate::{
    Error, Result,
    state::{Record, Scope},
};
use rdkafka::consumer::{BaseConsumer, Consumer};
use rdkafka::producer::{FutureProducer, FutureRecord, Producer};
use rdkafka::{ClientConfig, Message, Offset, TopicPartitionList};
use std::time::{Duration, Instant};

const TIMEOUT: Duration = Duration::from_secs(15);
pub struct Log {
    brokers: String,
    topic: String,
    producer: Option<FutureProducer>,
}
impl Log {
    pub fn reader(brokers: String, scope: &Scope) -> Self {
        Self {
            brokers,
            topic: scope.topic(),
            producer: None,
        }
    }
    /// Call only after acquiring a new catalog ownership lease. Never retry on
    /// the same owner after fencing or an uncertain transaction outcome.
    pub fn initialize_writer(&mut self) -> Result<()> {
        let producer: FutureProducer = ClientConfig::new()
            .set("bootstrap.servers", &self.brokers)
            .set("transactional.id", &self.topic)
            .set("enable.idempotence", "true")
            .set("acks", "all")
            .set("message.timeout.ms", "10000")
            .set("transaction.timeout.ms", "15000")
            .set("message.max.bytes", "2097152")
            .create()?;
        producer.init_transactions(TIMEOUT)?;
        self.producer = Some(producer);
        Ok(())
    }
    pub fn append(&self, record: &Record) -> Result<i64> {
        let producer = self
            .producer
            .as_ref()
            .ok_or_else(|| Error::Unavailable("read-only log".into()))?;
        let bytes = serde_json::to_vec(record)?;
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
    pub fn read(&self, after: i64, through: i64) -> Result<Vec<(i64, Record)>> {
        if through < after {
            return Err(Error::Conflict("snapshot is older than replica".into()));
        }
        if through == after {
            return Ok(vec![]);
        }
        let consumer: BaseConsumer = ClientConfig::new()
            .set("bootstrap.servers", &self.brokers)
            .set("group.id", format!("vdb-reader-{}", uuid::Uuid::new_v4()))
            .set("enable.auto.commit", "false")
            .set("enable.auto.offset.store", "false")
            .set("isolation.level", "read_committed")
            .set("auto.offset.reset", "error")
            .create()?;
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
        let (low, _) = consumer.fetch_watermarks(&self.topic, 0, TIMEOUT)?;
        if after + 1 < low {
            return Err(Error::Integrity(
                "WAL prefix missing; a newer checkpoint is required".into(),
            ));
        }
        let mut assignment = TopicPartitionList::new();
        assignment.add_partition_offset(&self.topic, 0, Offset::Offset(after + 1))?;
        consumer.assign(&assignment)?;
        let deadline = Instant::now() + TIMEOUT;
        let mut records = vec![];
        let mut bytes_read = 0usize;
        while Instant::now() < deadline {
            if let Some(message) = consumer.poll(Duration::from_millis(100)) {
                let message = message?;
                let offset = message.offset();
                if offset > through {
                    return Err(Error::Integrity("read barrier record is absent".into()));
                }
                let bytes = message
                    .payload()
                    .ok_or_else(|| Error::Integrity("empty WAL record".into()))?;
                bytes_read += bytes.len();
                if bytes_read > crate::state::MAX_HISTORY_BYTES {
                    return Err(Error::Unavailable("replay exceeds P1 memory limit".into()));
                }
                records.push((offset, serde_json::from_slice(bytes)?));
                if offset == through {
                    return Ok(records);
                }
            }
        }
        Err(Error::Unavailable(
            "timed out waiting for committed WAL prefix".into(),
        ))
    }
}
