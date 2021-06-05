#include <glog/logging.h>
#include "util.h"
#include "config.h"
#include "node.h"

namespace vectordb {

Node::Node()
    :meta_(Config::GetInstance().meta_path()) {
}

Node::~Node() {
}

Status
Node::Init() {
    Status s;
    if (!DirOK(Config::GetInstance().data_path())) {
        Mkdir(Config::GetInstance().data_path());
    }

    s = meta_.Init();
    assert(s.ok());

    s = grpc_server_.Init();
    assert(s.ok());

#if 0
    {
        std::string table_name;
        std::string str;
        table_name = "my_table1";
        str = Config::GetInstance().engine_path();
        str.append("/");
        str.append(table_name);
        s = meta_.AddTable(table_name, 1, 1, kVEngineAnnoy, str);
    }
    {
        std::string table_name;
        std::string str;
        table_name = "my_table2";
        str = Config::GetInstance().engine_path();
        str.append("/");
        str.append(table_name);
        s = meta_.AddTable(table_name, 5, 3, kVEngineAnnoy, str);
    }

    meta_.Persist();

    LOG(INFO) << "add table: " << meta_.ToStringShort();
    assert(s.ok());
#endif

    return Status::OK();
}

Status
Node::Start() {
    Status s;
    s = grpc_server_.Start();
    assert(s.ok());
    return Status::OK();
}

Status
Node::Stop() {
    Status s;
    s = grpc_server_.Stop();
    assert(s.ok());
    return Status::OK();
}

Status
Node::OnPing(const vectordb_rpc::PingRequest* request, vectordb_rpc::PingReply* reply) {
    if (request->msg() == "ping") {
        reply->set_msg("pang");
    } else {
        reply->set_msg("no_sound");
    }
    return Status::OK();
}

}  // namespace vectordb
