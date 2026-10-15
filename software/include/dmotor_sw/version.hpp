#pragma once
namespace dmotor_sw {
// Prints on every startup. Returns true for --version (caller exits successfully).
bool startup_version(const char* application, int argc, char** argv);
}
