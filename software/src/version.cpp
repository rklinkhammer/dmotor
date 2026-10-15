#include "dmotor_sw/version.hpp"
#include "dmotor_version_generated.hpp"
#include <iostream>
#include <cstring>
namespace dmotor_sw {
bool startup_version(const char* application, int argc, char** argv) {
    std::cout << application << ": " << DMOTOR_VERSION_BANNER << std::endl;
    return argc == 2 && std::strcmp(argv[1], "--version") == 0;
}
}
