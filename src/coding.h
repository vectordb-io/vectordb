#ifndef __VECTORDB_CODING_H__
#define __VECTORDB_CODING_H__

#include "meta.h"
#include "vectordb_meta.pb.h"

namespace vectordb {

void Pb2Replica(const vectordb_meta::Replica &pb, Replica &replica);
void Pb2Partition(const vectordb_meta::Partition &pb, Partition &partition);
void Pb2Table(const vectordb_meta::Table &pb, Table &table);

void Replica2Pb(const Replica &replica, vectordb_meta::Replica &pb);
void Partition2Pb(const Partition &partition, vectordb_meta::Partition &pb);
void Table2Pb(const Table &table, vectordb_meta::Table &pb);

void Replica2Str(const Replica &replica, std::string &s);
void Partition2Str(const Partition &partition, std::string &s);
void Table2Str(const Table &table, std::string &s);

bool Str2Replica(const std::string &s, Replica &replica);
bool Str2Partition(const std::string &s, Partition &partition);
bool Str2Table(const std::string &s, Table &table);

bool Str2TableNames(const std::string &s, std::vector<std::string> &table_names);
void TableNames2Str(const std::vector<std::string> &table_names, std::string &s);

}  // namespace vectordb

#endif
