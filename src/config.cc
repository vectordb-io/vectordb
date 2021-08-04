#include <cstdio>
#include <string>
#include "version.h"
#include "config.h"

namespace vectordb {

std::string
Config::ToString() const {
    jsonxx::json j;
    j["address"] = address_.ToString();
    j["data_path"] = data_path();
    j["meta_path"] = meta_path();
    j["engine_path"] = engine_path();
    return j.dump(4, ' ');
}

Status
Config::Load(int argc, char **argv) {
    int option_index, option_value;
    option_index = 0;
    static struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'v'},
        {"addr", required_argument, nullptr, 'a'},
        {"data_path", required_argument, nullptr, 'd'},
        {nullptr, 0, nullptr, 0}
    };

    while ((option_value = getopt_long(argc, argv, "hva:d:", long_options, &option_index)) != -1) {
        switch (option_value) {
        case 'a': {
            bool b = address_.ParseFromString(std::string(optarg));
            if (!b) {
                std::string msg = "parse host:port error, ";
                msg.append(std::string(optarg));
                return Status::OtherError(msg);
            }
            break;
        }

        case 'd': {
            data_path_ = std::string(optarg);
            break;
        }

        case 'v': {
            printf("%s\n", __VECTORDB__VERSION__);
            fflush(nullptr);
            exit(0);
            break;
        }

        case 'h': {
            return Status::Help("help");
            break;
        }

        default: {
            return Status::Help("help");
            break;
        }

        }
    }
    return Status::OK();
}

} // namespace vectordb
