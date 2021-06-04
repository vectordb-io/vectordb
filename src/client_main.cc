#include <random>
#include <string>
#include <thread>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include "vector_rpc.grpc.pb.h"




int main(int argc, char **argv) {
    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging(argv[0]);

    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:38000", grpc::InsecureChannelCredentials());
    std::unique_ptr<vector_rpc::VectorDB::Stub> stub = vector_rpc::VectorDB::NewStub(channel);

    vector_rpc::PingRequest request;
    vector_rpc::PingReply reply;
    request.set_msg("ping");
    grpc::ClientContext context;
    grpc::Status s = stub->Ping(&context, request, &reply);
    LOG(INFO) << reply.msg();

    return 0;
}
