#include "vdb_client.h"

namespace vectordb {

VdbClient::VdbClient(const std::string &address)
    :address_(address) {
}

VdbClient::~VdbClient() {
}

Status
VdbClient::Connect() {
    channel_ = grpc::CreateChannel(address_, grpc::InsecureChannelCredentials());
    stub_ = vectordb_rpc::VectorDB::NewStub(channel_);
    return Status::OK();
}

Status
VdbClient::Ping(vectordb_rpc::PingReply* reply) {
    vectordb_rpc::PingRequest request;
    request.set_msg("ping");
    auto s = Ping(request, reply);
    return s;
}

Status
VdbClient::CreateTable(const std::string &table_name, int dim, vectordb_rpc::CreateTableReply* reply) {
    vectordb_rpc::CreateTableRequest request;
    request.set_table_name(table_name);
    request.set_dim(dim);
    request.set_partition_num(1);
    request.set_replica_num(1);
    auto s = CreateTable(request, reply);
    return s;
}

Status
VdbClient::PutVec(const std::string &table_name,
                  const std::string &key,
                  const std::vector<float> &vec,
                  const std::vector<std::string> &attach_values,
                  vectordb_rpc::PutVecReply* reply) {
    vectordb_rpc::PutVecRequest request;
    request.set_table_name(table_name);
    request.mutable_vec_obj()->set_key(key);

    if (attach_values.size() >= 1) {
        request.mutable_vec_obj()->set_attach_value1(attach_values[0]);
    }
    if (attach_values.size() >= 2) {
        request.mutable_vec_obj()->set_attach_value2(attach_values[1]);
    }
    if (attach_values.size() >= 3) {
        request.mutable_vec_obj()->set_attach_value3(attach_values[2]);
    }

    for (auto &d: vec) {
        request.mutable_vec_obj()->mutable_vec()->add_data(d);
    }

    auto s = PutVec(request, reply);
    return s;
}

Status
VdbClient::BuildIndex(const std::string &table_name, vectordb_rpc::BuildIndexReply* reply) {
    vectordb_rpc::BuildIndexRequest request;
    request.set_table_name(table_name);
    request.set_index_type("annoy");
    auto s = BuildIndex(request, reply);
    return s;
}

Status
VdbClient::GetKNN(const std::string &table_name, const std::string &key, int limit, vectordb_rpc::GetKNNReply* reply) {
    vectordb_rpc::GetKNNRequest request;
    request.set_table_name(table_name);
    request.set_key(key);
    request.set_limit(limit);
    request.set_index_name("default");

    auto s = GetKNN(request, reply);
    return s;
}

Status
VdbClient::Ping(const vectordb_rpc::PingRequest &request, vectordb_rpc::PingReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->Ping(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::Info(const vectordb_rpc::InfoRequest &request, vectordb_rpc::InfoReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->Info(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::CreateTable(const vectordb_rpc::CreateTableRequest &request, vectordb_rpc::CreateTableReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->CreateTable(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::ShowTables(const vectordb_rpc::ShowTablesRequest &request, vectordb_rpc::ShowTablesReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->ShowTables(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::Describe(const vectordb_rpc::DescribeRequest &request, vectordb_rpc::DescribeReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->Describe(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::PutVec(const vectordb_rpc::PutVecRequest &request, vectordb_rpc::PutVecReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->PutVec(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::GetVec(const vectordb_rpc::GetVecRequest &request, vectordb_rpc::GetVecReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->GetVec(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::DistKey(const vectordb_rpc::DistKeyRequest &request, vectordb_rpc::DistKeyReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->DistKey(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::Keys(const vectordb_rpc::KeysRequest &request, vectordb_rpc::KeysReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->Keys(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::BuildIndex(const vectordb_rpc::BuildIndexRequest &request, vectordb_rpc::BuildIndexReply* reply) {
    grpc::ClientContext context;
    grpc::Status status = stub_->BuildIndex(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

Status
VdbClient::GetKNN(const vectordb_rpc::GetKNNRequest &request, vectordb_rpc::GetKNNReply* reply) {

    grpc::ClientContext context;
    grpc::Status status = stub_->GetKNN(&context, request, reply);
    if (!status.ok()) {
        std::string reply_msg = status.error_message();
        return Status::OtherError(reply_msg);
    }
    return Status::OK();
}

} // namespace vectordb
