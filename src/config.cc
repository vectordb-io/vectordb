#include "config.h"

namespace vectordb {

Config::Config() {
}

Config::~Config() {
}

std::string
Config::DebugString() {
    std::string s;
    s.append("\n[\n");
    s.append("address: \n");
    s.append("]\n");
    return s;
}

Status
Config::Load(int argc, char **argv) {
    int option_index, option_value;
    option_index = 0;
    static struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"ping", no_argument, nullptr, 't'},  // t means test
        {"peers", required_argument, nullptr, 'p'},
        {"path", required_argument, nullptr, 's'},  // s means storage path
        {"me", required_argument, nullptr, 'm'},
        {nullptr, 0, nullptr, 0}
    };

    vectordb::HostAndPort me;
    std::vector<vectordb::HostAndPort> peers;
    while ((option_value = getopt_long(argc, argv, "hp:m:", long_options, &option_index)) != -1) {
        switch (option_value) {
        case 'm':
            break;

        case 't':
            break;

        case 's':
            storage_path_ = std::string(optarg);
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
Config::ParseHostPort(std::string &hp, std::string &host, int &port) {
    char* psave = nullptr;
    const char *d = ":";
    char *p;
    p = strtok_r((char*)hp.c_str(), d, &psave);
    host = std::string(p);
    p = strtok_r(nullptr, d, &psave);
    sscanf(p, "%d", &port);
}

} // namespace vectordb
