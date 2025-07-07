#ifndef VECTORDB_RETNO_H
#define VECTORDB_RETNO_H

#include <string>

namespace vectordb {

enum RetNo {
  RET_OK = 0,
  RET_NOT_FOUND,
  RET_ERROR,
};

inline std::string RetNoToString(RetNo retno) {
  switch (retno) {
    case RET_OK:
      return "ok";
    case RET_NOT_FOUND:
      return "not found";
    case RET_ERROR:
      return "error";
    default:
      return "unknown";
  }
}

}  // namespace vectordb

#endif
