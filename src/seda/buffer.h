#ifndef VRAFT_BUFFER_H_
#define VRAFT_BUFFER_H_

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "coding.h"

namespace vraft {

#define BUF_INIT_BYTES (1024 * 64)
#define BUF_MAX_WASTE_BYTES (BUF_INIT_BYTES / 2)

// buf.size() make no sense!!!
// just use BeginRead(), BeginWrite()
class Buffer final {
 public:
  Buffer(int32_t init_bytes, int32_t max_waste_bytes);
  Buffer();
  ~Buffer();
  Buffer(const Buffer &t) = delete;
  Buffer &operator=(const Buffer &t) = delete;

  void Move();
  void Reset();
  void MaybeMove();
  void MaybeReset();
  void RetrieveInt8();
  void RetrieveInt16();
  void RetrieveInt32();
  void RetrieveInt64();
  void RetrieveAll();
  void Retrieve(int32_t len);
  void MakeSpace(int32_t len);
  void EnsureWritableBytes(int32_t len);
  void Append(const char *data, size_t len);

  int32_t ReadableBytes() const;
  int32_t WritableBytes() const;
  int32_t WastefulBytes() const;

  const char *BeginRead() const;
  const char *BeginWrite() const;
  const char *Peek() const;
  char *BeginWrite();

  int8_t PeekInt8() const;
  int16_t PeekInt16() const;
  int32_t PeekInt32() const;
  int64_t PeekInt64() const;

 public:  // for test
  int32_t init_bytes() const { return init_bytes_; }
  int32_t max_waste_bytes() const { return max_waste_bytes_; }
  const std::vector<char> &buf() const { return buf_; }
  int32_t read_index() const { return read_index_; }
  int32_t write_index() const { return write_index_; }

 private:
  char *Begin();
  const char *Begin() const;

 private:
  int32_t init_bytes_;
  int32_t max_waste_bytes_;
  std::vector<char> buf_;

  int32_t read_index_;
  int32_t write_index_;
};

inline Buffer::Buffer(int32_t init_bytes, int32_t max_waste_bytes)
    : init_bytes_(init_bytes),
      max_waste_bytes_(max_waste_bytes),
      read_index_(0),
      write_index_(0) {
  assert(max_waste_bytes > 0);
  assert(init_bytes > max_waste_bytes);
  buf_.reserve(init_bytes);
}

inline Buffer::Buffer() : Buffer(BUF_INIT_BYTES, BUF_MAX_WASTE_BYTES) {}

inline Buffer::~Buffer() {}

inline void Buffer::Reset() {
  read_index_ = 0;
  write_index_ = 0;
}

inline void Buffer::MaybeMove() {
  if (WastefulBytes() > max_waste_bytes_) {
    Move();
  }
}

inline void Buffer::Move() {
  if (WastefulBytes() > 0) {
    int32_t readable = ReadableBytes();
    std::copy(Begin() + read_index_, Begin() + write_index_, Begin());

    read_index_ = 0;
    write_index_ = readable;
    assert(readable == ReadableBytes());
  }
}

inline void Buffer::MaybeReset() {
  if (read_index_ == write_index_ && read_index_ > 0) {
    Reset();
  }
}

inline void Buffer::RetrieveInt8() { Retrieve(sizeof(int8_t)); }

inline void Buffer::RetrieveInt16() { Retrieve(sizeof(int16_t)); }

inline void Buffer::RetrieveInt32() { Retrieve(sizeof(int32_t)); }

inline void Buffer::RetrieveInt64() { Retrieve(sizeof(int64_t)); }

inline void Buffer::RetrieveAll() { Reset(); }

inline void Buffer::Retrieve(int32_t len) {
  assert(len <= ReadableBytes());
  read_index_ += len;

  MaybeReset();
  MaybeMove();
}

inline void Buffer::EnsureWritableBytes(int32_t len) {
  if (WritableBytes() < len) {
    MakeSpace(len);
  }
  assert(WritableBytes() >= len);
}

inline void Buffer::Append(const char *data, size_t len) {
  EnsureWritableBytes(len);
  std::copy(data, data + len, BeginWrite());
  write_index_ += len;
}

inline void Buffer::MakeSpace(int32_t len) {
  if (WritableBytes() < len) {
    buf_.resize(write_index_ + len);
  }
}

inline char *Buffer::Begin() { return &(*buf_.begin()); }

inline const char *Buffer::Begin() const { return &(*buf_.begin()); }

inline const char *Buffer::BeginRead() const { return Begin() + read_index_; }

inline const char *Buffer::BeginWrite() const { return Begin() + write_index_; }

inline char *Buffer::BeginWrite() { return Begin() + write_index_; }

inline const char *Buffer::Peek() const { return BeginRead(); }

inline int8_t Buffer::PeekInt8() const {
  assert(ReadableBytes() >= static_cast<int32_t>(sizeof(int8_t)));
  int8_t i8 = DecodeFixed8(Peek());
  return i8;
}

inline int16_t Buffer::PeekInt16() const {
  assert(ReadableBytes() >= static_cast<int32_t>(sizeof(int16_t)));
  int16_t i16 = DecodeFixed16(Peek());
  return i16;
}

inline int32_t Buffer::PeekInt32() const {
  assert(ReadableBytes() >= static_cast<int32_t>(sizeof(int32_t)));
  int32_t i32 = DecodeFixed32(Peek());
  return i32;
}

inline int64_t Buffer::PeekInt64() const {
  assert(ReadableBytes() >= static_cast<int32_t>(sizeof(int64_t)));
  int64_t i64 = DecodeFixed64(Peek());
  return i64;
}

inline int32_t Buffer::ReadableBytes() const {
  return write_index_ - read_index_;
}

inline int32_t Buffer::WritableBytes() const {
  return buf_.capacity() - write_index_;
}

inline int32_t Buffer::WastefulBytes() const { return read_index_; }

}  // namespace vraft

#endif
