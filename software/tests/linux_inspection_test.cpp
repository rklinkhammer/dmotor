#include "dmotor_sw/version.hpp"
#include "dmotor_sw/linux_inspection.hpp"
#include "dmotor_sw/protocol.hpp"
#include "dmotor/controller_registers.hpp"
#include <cstdlib>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
namespace reg = dmotor::controller_reg;
using namespace dmotor_sw;
template<class F> bool fails(F operation) { try { operation(); } catch (const IoError&) { return true; } return false; }
int main(int argc, char** argv) {
    if (dmotor_sw::startup_version("dmotor-linux-tests", argc, argv)) return 0;
    char name[] = "/tmp/dmotor-inspection-XXXXXX";
    const int seed = mkstemp(name); CHECK(seed >= 0); CHECK(ftruncate(seed, 65536) == 0);
    auto put = [&](uint32_t address, uint32_t value) { CHECK(pwrite(seed, &value, 4, address) == 4); };
    auto open_file = [&]() { return open(name, O_RDWR | O_CLOEXEC); };
    put(reg::IDENTITY, reg::PROFILE_ID);
    {
        LinuxInspection device(open_file(), 65536, 0); Protocol protocol(device);
        CHECK(protocol.compatible_controller()); CHECK(protocol.status().enabled == 0);
        CHECK(fails([&] { LinuxInspection second(open_file(), 65536, 0); }));
        const pid_t child = fork(); CHECK(child >= 0);
        if (child == 0) _exit(fails([&] { LinuxInspection second(open_file(), 65536, 0); }) ? 0 : 1);
        int status = 0; CHECK(waitpid(child, &status, 0) == child); CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(fails([&] { device.write32(reg::ENABLE, 1); }));
        CHECK(fails([&] { device.write32(reg::CORE_RESET, 1); }));
        CHECK(fails([&] { device.write32(reg::SNAPSHOT, 2); }));
        CHECK(fails([&] { device.read32(0); })); // Blocked mailbox never touched.
        CHECK(fails([&] { device.read32(reg::TELEMETRY_BASE + 1); }));
        CHECK(fails([&] { device.read32(UINT32_MAX); }));
        put(reg::TELEMETRY_BASE + 16 * 4, 0xffffffff);
        put(reg::TELEMETRY_BASE + 17 * 4, 0xffffffff);
        TelemetrySnapshot snapshot; CHECK(protocol.capture(snapshot)); CHECK(snapshot.motors[0].position_counts == -1);
        put(reg::STATUS, 1); CHECK(fails([&] { device.read32(reg::STATUS); }));
        CHECK(fails([&] { device.write32(reg::SNAPSHOT, 1); }));
        put(reg::STATUS, 0); put(reg::IDENTITY, 0); CHECK(fails([&] { device.fence(); }));
    }
    // Constructor rejection releases the lock and descriptor.
    CHECK(fails([&] { LinuxInspection wrong(open_file(), 65536, 0); }));
    put(reg::IDENTITY, reg::PROFILE_ID);
    { LinuxInspection again(open_file(), 65536, 0); }
    CHECK(fails([&] { LinuxInspection small(open_file(), 4096, 0); }));
    CHECK(fails([&] { LinuxInspection unaligned(open_file(), 65536, 1); }));
    CHECK(fails([&] { LinuxInspection truncated(open_file(), 131072, 0); }));
    put(reg::STATUS, 1); CHECK(fails([&] { LinuxInspection enabled(open_file(), 65536, 0); }));
    put(reg::STATUS, 0);
    // Nonzero UIO map offset must be included in every register access.
    put(reg::IDENTITY + 128, reg::PROFILE_ID); put(reg::STATUS + 128, 0);
    { LinuxInspection shifted(open_file(), 65536, 128); CHECK(shifted.read32(reg::IDENTITY) == reg::PROFILE_ID); }
    close(seed); CHECK(unlink(name) == 0);
    CHECK(fails([&] { LinuxInspection missing(-1, 65536, 0); }));
    std::cout << "PASS: Linux mapping, cross-process ownership, inspection allowlists and cleanup\n";
}
