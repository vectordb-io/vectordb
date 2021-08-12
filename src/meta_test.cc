#include <sys/syscall.h>
#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <glog/logging.h>
#include "status.h"
#include "config.h"
#include "util.h"
#include "meta.h"

#define gettid() (syscall(SYS_gettid))


std::string exe_name;

vectordb::Meta *g_meta;

void PrintHelp() {
    std::cout << std::endl;
    std::cout << "Usage: " << std::endl << std::endl;
    std::cout << exe_name << " --addr=127.0.0.1:38000 --data_path=/tmp/test_meta" << std::endl;
    std::cout << exe_name << " -h" << std::endl;
    std::cout << exe_name << " --help" << std::endl;
    std::cout << exe_name << " -v" << std::endl;
    std::cout << exe_name << " --version" << std::endl;
    std::cout << std::endl;
}

void AddTable(const std::string table_name, int partition_num, int replica_num, int dim) {
    printf("tid:%ld AddTable: %s \n", gettid(), table_name.c_str());
    fflush(nullptr);

    vectordb::TableParam table_param;
    table_param.name = table_name;
    table_param.partition_num = partition_num;
    table_param.replica_num = replica_num;
    table_param.path = "/xx/test_meta_path";
    table_param.dim = dim;

    auto s = g_meta->AddTable(table_param);
    if (!s.ok()) {
        printf("%s %s \n", table_name.c_str(), s.ToString().c_str());
        assert(0);
    }

    s = g_meta->Persist();
    assert(s.ok());

    printf("create %s finish \n", table_name.c_str());
    fflush(nullptr);
}

int main(int argc, char** argv) {
    exe_name = std::string(argv[0]);

    FLAGS_alsologtostderr = true;
    FLAGS_logbufsecs = 0;
    FLAGS_max_log_size = 10;
    google::InitGoogleLogging(argv[0]);

    if (argc < 2) {
        PrintHelp();
        exit(0);
    }

    auto s = vectordb::Config::GetInstance().Load(argc, argv);
    if (s.ok()) {
        LOG(INFO) << "read config: \n" << vectordb::Config::GetInstance().ToString();
    } else {
        LOG(INFO) << "read config error: " << s.ToString();
        PrintHelp();
        exit(-1);
    }

    vectordb::util::RecurMakeDir(vectordb::Config::GetInstance().data_path());
    if (!vectordb::util::DirOK(vectordb::Config::GetInstance().data_path())) {
        std::cout << "dir error: " << vectordb::Config::GetInstance().data_path() << std::endl;
        return -1;
    }

    vectordb::Meta meta(vectordb::Config::GetInstance().meta_path());
    g_meta = &meta;

    s = meta.Init();
    assert(s.ok());
    LOG(INFO) << meta.ToStringPretty();

    AddTable("test_table", 10, 2, 512);
    LOG(INFO) << meta.ToStringPretty();

    auto table_sp = meta.GetTable("test_table");
    assert(table_sp);

    std::string index_name = vectordb::util::IndexName("test_table", "annoy", time(nullptr));
    table_sp->mutable_indices().push_back(index_name);

    /*
    int thread_num = 5;
    //int thread_num = 100;
    std::vector<std::thread*> threads;
    for (int i = 0; i < thread_num; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "test_table_%d", i);
        std::string table_name(buf);
        printf("begin create table: %s \n", table_name.c_str());
        fflush(nullptr);
        std::thread *t = new std::thread(AddTable, table_name, 5, 3, 512);
        //std::thread *t = new std::thread(AddTable, table_name, 1024, 3, 512);
        threads.push_back(t);
    }

    for (auto &t : threads) {
        t->join();
    }
    */

    s = meta.Persist();
    assert(s.ok());
    LOG(INFO) << "meta persist ok!";
    LOG(INFO) << meta.ToStringPretty();

    printf("\nReplicaNamesByTable test: \n\n");
    std::vector<std::string> replica_names;

    s = meta.ReplicaNamesByTable("test_table_0", replica_names);
    std::cout << "table: test_table_0, size:" << replica_names.size() << " status:" << s.ToString() << std::endl;
    for (auto &rn : replica_names) {
        std::cout << rn << std::endl;
        std::cout.flush();
    }

    s = meta.ReplicaNamesByTable("test_table_1", replica_names);
    std::cout << "table: test_table_1, size:" << replica_names.size() << " status:" << s.ToString() << std::endl;
    for (auto &rn : replica_names) {
        std::cout << rn << std::endl;
        std::cout.flush();
    }


    google::ShutdownGoogleLogging();
    return 0;
}
