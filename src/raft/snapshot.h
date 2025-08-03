#ifndef VRAFT_SNAPSHOT_H_
#define VRAFT_SNAPSHOT_H_

#include "common.h"
#include "raft_addr.h"

namespace vraft {

using CreateReaderFunc =
    std::function<SnapshotReaderSPtr(std::string &path, int32_t max_read)>;

using CreateWriterFunc = std::function<SnapshotWriterSPtr(
    std::string path, RaftIndex last_index, RaftTerm last_term)>;

class SnapshotReader {
 public:
  SnapshotReader(std::string path, int32_t max_read)
      : path_(path), offset_(0), done_(false), max_read_(max_read) {}
  virtual ~SnapshotReader() {}
  SnapshotReader(const SnapshotReader &) = delete;
  SnapshotReader &operator=(const SnapshotReader &) = delete;

  std::string path() { return path_; }
  RaftIndex last_index() { return last_index_; }
  RaftTerm last_term() { return last_term_; }
  int32_t offset() { return offset_; }
  const std::string &data() const { return data_; };
  bool done() { return done_; }

  virtual int32_t Start() = 0;
  virtual int32_t Read() = 0;  // -1:error, 0:finish, >0:offset
  virtual int32_t Finish() = 0;

 protected:
  std::string path_;
  RaftIndex last_index_;
  RaftTerm last_term_;

  std::string data_;  // read data
  int32_t offset_;    // update by Read
  bool done_;         // update by Read

  int32_t max_read_;
};

class SnapshotWriter {
 public:
  SnapshotWriter(std::string path, RaftIndex last_index, RaftTerm last_term)
      : path_(path),
        last_index_(last_index),
        last_term_(last_term),
        stored_(0) {}
  virtual ~SnapshotWriter() {}
  SnapshotWriter(const SnapshotWriter &) = delete;
  SnapshotWriter &operator=(const SnapshotWriter &) = delete;

  std::string path() { return path_; }
  RaftIndex last_index() { return last_index_; }
  RaftTerm last_term() { return last_term_; }
  int32_t stored() { return stored_; }

  virtual int32_t Start() = 0;
  virtual int32_t Write(const std::string &data) = 0;  // -1:error, 0:ok
  virtual int32_t Finish() = 0;

 protected:
  std::string path_;
  RaftIndex last_index_;
  RaftTerm last_term_;

  int32_t stored_;
};

struct Snapshot {
  SnapshotReaderSPtr reader_;
  SnapshotWriterSPtr writer_;
};

}  // namespace vraft

#endif
