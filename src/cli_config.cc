#include <getopt.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "version.h"
#include "cli_config.h"

namespace vectordb {

std::string
CliConfig::address() const {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s:%d", host_.c_str(), port_);
    return std::string(buf);
}

Status
CliConfig::Load(int argc, char **argv) {
    int option_index, option_value;
    option_index = 0;
    static struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'v'},
        {"addr", required_argument, nullptr, 'a'},
        {nullptr, 0, nullptr, 0}
    };

    while ((option_value = getopt_long(argc, argv, "hva:d:", long_options, &option_index)) != -1) {
        switch (option_value) {
        case 'a': {
            ParseHostPort(std::string(optarg), host_, port_);
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

void
CliConfig::ParseHostPort(const std::string &hp, std::string &host, int &port) {
    char* psave = nullptr;
    const char *d = ":";
    char *p;
    p = strtok_r((char*)hp.c_str(), d, &psave);
    host = std::string(p);
    p = strtok_r(nullptr, d, &psave);
    sscanf(p, "%d", &port);
}

} // namespace vectordb
