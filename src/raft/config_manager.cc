#include "config_manager.h"

#include "allocator.h"

namespace vraft {

bool RaftConfig::operator==(const RaftConfig& rhs) const {
  if (!(me == rhs.me)) {
    return false;
  }

  if (peers.size() != rhs.peers.size()) {
    return false;
  }

  for (size_t i = 0; i < peers.size(); ++i) {
    if (!(peers[i] == rhs.peers[i])) {
      return false;
    }
  }

  return true;
}

int32_t RaftConfig::MaxBytes() {
  int32_t size = 0;
  size += sizeof(uint64_t);      // RaftAddr me
  size += sizeof(uint32_t) * 2;  // std::vector<RaftAddr> peers;
  for (size_t i = 0; i < peers.size(); ++i) {
    size += sizeof(uint64_t);
  }
  return size;
}

int32_t RaftConfig::ToString(std::string& s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char* ptr = reinterpret_cast<char*>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

int32_t RaftConfig::ToString(const char* ptr, int32_t len) {
  char* p = const_cast<char*>(ptr);
  int32_t size = 0;
  uint64_t u64 = 0;

  u64 = me.ToU64();
  EncodeFixed64(p, u64);
  p += sizeof(u64);
  size += sizeof(u64);

  int32_t peers_size = peers.size();
  EncodeFixed32(p, peers_size);
  p += sizeof(peers_size);
  size += sizeof(peers_size);

  for (int32_t i = 0; i < peers_size; ++i) {
    u64 = peers[i].ToU64();
    EncodeFixed64(p, u64);
    p += sizeof(u64);
    size += sizeof(u64);
  }

  assert(size <= len);
  return size;
}

int32_t RaftConfig::FromString(std::string& s) {
  return FromString(s.c_str(), s.size());
}

int32_t RaftConfig::FromString(const char* ptr, int32_t len) {
  char* p = const_cast<char*>(ptr);
  uint64_t u64 = 0;
  int32_t size = 0;
  int32_t len2 = len;

  u64 = DecodeFixed64(p);
  me.FromU64(u64);
  p += sizeof(u64);
  size += sizeof(u64);
  len2 -= sizeof(u64);

  int32_t peers_size = DecodeFixed32(p);
  p += sizeof(peers_size);
  size += sizeof(peers_size);
  len2 -= sizeof(peers_size);

  for (int32_t i = 0; i < peers_size; ++i) {
    u64 = DecodeFixed64(p);
    RaftAddr peer(u64);
    p += sizeof(u64);
    size += sizeof(u64);
    len2 -= sizeof(u64);
    peers.push_back(peer);
  }
  assert(len2 >= 0);

  return size;
}

void ConfigManager::set_current_cb(Functor cb) { current_cb_ = cb; }

void ConfigManager::RunCb() {
  if (current_cb_) {
    current_cb_();
  }
}

}  // namespace vraft
