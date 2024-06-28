#ifndef VECTORDB_VDB_MESSAGE_H_
#define VECTORDB_VDB_MESSAGE_H_

#include "message.h"

namespace vectordb {

enum VdbMsgType {
  kVersion = 100,
  kVersionReply,
};

}  // namespace vectordb

#endif
