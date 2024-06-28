#include <gtest/gtest.h>

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "coding.h"
#include "leveldb/db.h"
#include "util.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << argv[0] << " db_path" << std::endl;
    return 0;
  }
  vraft::CodingInit();
  std::string path = argv[1];

  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = false;
  options.error_if_exists = false;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  assert(status.ok());

  leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::string key = vraft::StrToHexStr(it->key().ToString().c_str(),
                                         it->key().ToString().size());
    std::string value = vraft::StrToHexStr(it->value().ToString().c_str(),
                                           it->value().ToString().size());
    std::cout << key << " -- " << value << std::endl;
  }
  assert(it->status().ok());  // Check for any errors found during the scan
  delete it;
  delete db;

  return 0;
}