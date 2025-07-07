#ifndef VECTORDB_OPTIONS_H
#define VECTORDB_OPTIONS_H

namespace vectordb {

struct WOptions {
  bool write_vector_to_data = true;
  bool write_vector_to_index = true;
};

struct ROptions {};

}  // namespace vectordb

#endif  // VECTORDB_OPTIONS_H