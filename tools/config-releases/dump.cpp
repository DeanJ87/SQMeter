#include "Config.h"
#include <cstdio>
using namespace SQM;
// Prints the config a release would store for a typical set-up device.
int main(int, char **argv)
{
    Config base = Config::createDefault();
    std::string error;
    auto cfg = Config::fromJson(argv[1], &base);
    if (!cfg)
    {
        fprintf(stderr, "rejected\n");
        return 1;
    }
    printf("%s\n", cfg->toJson(false).c_str());
}
