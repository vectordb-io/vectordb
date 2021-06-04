#include <cstdio>
#include <string>
#include "version.h"
#include "config.h"

namespace vectordb {

Config::Config() {
}

Config::~Config() {
}

std::string
Config::ToString() const {
    std::string s;
    s.append("[\n");
    s.append("address:");
    s.append(address_.ToString());
    s.append("\n");
    s.append("data_path:");
    s.append(data_path_);
    s.append("\n");
    s.append("]\n");
    return s;
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
            std::string host;
            int port;
            ParseHostPort(std::string(optarg), host, port);
            address_.set_host(host);
            address_.set_port(port);
            break;
        }

        case 'd':
            data_path_ = std::string(optarg);
            break;

        case 'v':
            printf("%s\n", __VECTORDB__VERSION__);
            fflush(nullptr);
            exit(0);
            break;

        case 'h':
            return Status::InvalidArgument("-h", "help");
            exit(0);

        default:
            return Status::InvalidArgument("-h", "help");
            exit(0);
        }
    }
    return Status::OK();
}

void
Config::ParseHostPort(const std::string &hp, std::string &host, int &port) {
    char* psave = nullptr;
    const char *d = ":";
    char *p;
    p = strtok_r((char*)hp.c_str(), d, &psave);
    host = std::string(p);
    p = strtok_r(nullptr, d, &psave);
    sscanf(p, "%d", &port);
}

} // namespace vectordb
