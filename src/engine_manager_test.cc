#include <sys/syscall.h>
#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <glog/logging.h>
#include "status.h"
#include "config.h"
#include "meta.h"
#include "engine_manager.h"

#define gettid() (syscall(SYS_gettid))


std::string exe_name;

vectordb::Meta *g_meta;
vectordb::EngineManager *g_em;

void PrintHelp() {
    std::cout << std::endl;
    std::cout << "Usage: " << std::endl << std::endl;
    std::cout << exe_name << " --addr=127.0.0.1:38000 --data_path=/tmp/test_add_engine_by_meta" << std::endl;
    std::cout << exe_name << " -h" << std::endl;
    std::cout << exe_name << " --help" << std::endl;
    std::cout << exe_name << " -v" << std::endl;
    std::cout << exe_name << " --version" << std::endl;
    std::cout << std::endl;
}

vectordb::Status AddEngine(std::shared_ptr<vectordb::Table> t, std::shared_ptr<vectordb::Partition> p, std::shared_ptr<vectordb::Replica> r) {

    vectordb::VEngineParam param;
    param.dim = t->dim();
    param.replica_name = r->name();
    std::string path = r->path();

    auto vengine_sp = std::make_shared<vectordb::VEngine>(path, param);
    assert(vengine_sp);
    auto s = vengine_sp->Init();
    if (!s.ok()) {
        printf("%s \n", s.ToString().c_str());
        return s;
    }
    printf("add vengine: %s \n", vengine_sp->ToString().c_str());
    g_em->AddVEngine(vengine_sp->replica_name(), vengine_sp);

    return vectordb::Status::OK();
}

// test EngineManager::Init, load engine from meta

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

    vectordb::EngineManager engine_manager;
    g_em = &engine_manager;

    s = meta.ForEachReplica(std::bind(&vectordb::EngineManager::LoadEngine, &engine_manager, std::placeholders::_1));
    if (!s.ok()) {
        std::string msg = "engien manager init error: ";
        msg.append(s.ToString());
        return -1;
    }
    printf("%s \n", engine_manager.ToStringPretty().c_str());

    google::ShutdownGoogleLogging();
    return 0;
}
