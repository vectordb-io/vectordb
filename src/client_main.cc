#include <random>
#include <string>
#include <thread>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include "vector_rpc.grpc.pb.h"


std::default_random_engine random_(time(nullptr));


int main(int argc, char **argv) {
    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging(argv[0]);

    /*
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:38000", grpc::InsecureChannelCredentials());
    std::unique_ptr<vectordb_rpc::VPaxos::Stub> stub = vectordb_rpc::VPaxos::NewStub(channel);

    vectordb_rpc::Ping request;
    vectordb_rpc::PingReply reply;
    request.set_msg("ping");
    request.set_address("haha");

    grpc::ClientContext context;
    grpc::Status s = stub->RpcPing(&context, request, &reply);

    LOG(INFO) << "receive from " << reply.address() << ": " << reply.msg();
    */

    //std::thread t3(std::bind(Propose, "127.0.0.1:38002", "2222"));
    //t3.join();


    /*
    std::thread t1(std::bind(Propose, "127.0.0.1:38000", "0000"));
    std::thread t2(std::bind(Propose, "127.0.0.1:38001", "1111"));
    std::thread t3(std::bind(Propose, "127.0.0.1:38002", "2222"));
    std::thread t4(std::bind(Propose, "127.0.0.1:38003", "3333"));
    std::thread t5(std::bind(Propose, "127.0.0.1:38004", "4444"));

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    */


    return 0;
}
