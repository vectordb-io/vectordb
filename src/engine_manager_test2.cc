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
vectordb::EngineManager *g_em;

void PrintHelp() {
    std::cout << std::endl;
    std::cout << "Usage: " << std::endl << std::endl;
    std::cout << exe_name << " --addr=127.0.0.1:38000 --data_path=/tmp/test_engine_manager2" << std::endl;
    std::cout << exe_name << " -h" << std::endl;
    std::cout << exe_name << " --help" << std::endl;
    std::cout << exe_name << " -v" << std::endl;
    std::cout << exe_name << " --version" << std::endl;
    std::cout << std::endl;
}

// test EngineManager::LoadEngineByPath

int main(int argc, char** argv) {
    exe_name = std::string(argv[0]);

    FLAGS_alsologtostderr = true;
    FLAGS_logbufsecs = 0;
    FLAGS_max_log_size = 10;
    google::InitGoogleLogging(argv[0]);

    std::string path = "/tmp/test_engine_manager";
    std::string path2 = "/tmp/test_engine_manager2";

    // when leave region, auto closed
    {
        vectordb::VEngineParam param;
        param.dim = 10;
        param.replica_name = "test_replica";

        auto vengine_sp = std::make_shared<vectordb::VEngine>(path, param);
        assert(vengine_sp);
        auto s = vengine_sp->Init();
        if (!s.ok()) {
            printf("%s \n", s.ToString().c_str());
            return -1;
        }
        printf("vengine: %s \n", vengine_sp->ToString().c_str());
    }

    {
        vectordb::VEngineParam param;
        param.dim = 512;
        param.replica_name = "test_replica2";

        auto vengine_sp = std::make_shared<vectordb::VEngine>(path2, param);
        assert(vengine_sp);
        auto s = vengine_sp->Init();
        if (!s.ok()) {
            printf("%s \n", s.ToString().c_str());
            return -1;
        }
        printf("vengine: %s \n", vengine_sp->ToString().c_str());
    }

    vectordb::EngineManager engine_manager;
    g_em = &engine_manager;
    auto s = engine_manager.LoadEngineByPath(path);
    assert(s.ok());
    s = engine_manager.LoadEngineByPath(path2);
    assert(s.ok());

    printf("%s \n", engine_manager.ToStringPretty().c_str());

    google::ShutdownGoogleLogging();
    return 0;
}
