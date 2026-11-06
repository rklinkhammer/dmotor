#include "dmotor_sw/recovery_coordinator.hpp"
#include "dmotor_sw/endpoint_session.hpp"
#include "dmotor/controller_registers.hpp"
#include <cstdlib>
#include <iostream>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
using namespace dmotor_sw;
namespace reg = dmotor::controller_reg;
namespace b = dmotor::batch_reg;
#include "support/runtime_fixture.hpp"
using namespace dmotor_sw::recovery;
struct Directory {
    std::string path;
    Directory() {
        char name[] = "/tmp/dmotor-coordinator-session-XXXXXX";
        auto p = mkdtemp(name);
        CHECK(p);
        path = p;
    }
    ~Directory() {
        unlink((path + "/endpoint-sessions.log").c_str());
        rmdir(path.c_str());
    }
};
static Fake* observed = nullptr;
static unsigned syncs = 0, fail_sync = 0;
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    if (observed) {
        CHECK(observed->source && !(observed->words[reg::STATUS] & 15));
        ++syncs;
        if (syncs == fail_sync) {
            errno = EIO;
            return -1;
        }
    }
    return __real_fsync(fd);
}
int main() {
    Directory store;
    provision_endpoint_sessions(store.path, true);
    {
        Fake io;
        Runtime runtime(io);
        unsigned allocations = 0;
        Coordinator c(runtime, [&] {
            CHECK(io.source);
            ++allocations;
            return allocate_endpoint_session(store.path, true);
        });
        CHECK(!monitor::valid(c.discover()));
        observed = &io;
        syncs = 0;
        c.prepare(1, config(), 1);
        CHECK(syncs == 2 && allocations == 1 && !io.source && runtime.recovery_held());
        auto hello = c.discover();
        CHECK(monitor::valid(hello) && hello.source == 1);
        CHECK(c.discover().sequence == hello.sequence && c.discover().source == 1 &&
              allocations == 1);
        c.cancel();
        CHECK(io.source && !monitor::valid(c.discover()));
        c.prepare(1, config(), 1);
        CHECK(c.discover().source == 2);
        observed = nullptr;
    }
    {
        Fake io;
        Runtime runtime(io);
        Coordinator restarted(runtime, [&] {
            CHECK(io.source);
            return allocate_endpoint_session(store.path, true);
        });
        restarted.prepare(2, config(), 1);
        CHECK(restarted.discover().source == 3);
    }
    for (unsigned fail : {1U, 2U}) {
        Directory bad;
        provision_endpoint_sessions(bad.path, true);
        Fake io;
        Runtime runtime(io);
        Coordinator c(runtime, [&] { return allocate_endpoint_session(bad.path, true); });
        observed = &io;
        syncs = 0;
        fail_sync = fail;
        CHECK(rejects([&] { c.prepare(1, config(), 1); }));
        CHECK(io.source && runtime.recovery_held() && !c.admitted());
        CHECK(!monitor::valid(c.discover()) && !monitor::valid(c.poll(io.time)));
        CHECK(io.words[reg::SESSION] == 0); // No PL preparation after storage failure.
        fail_sync = 0;
        syncs = 0;
        c.prepare(1, config(), 1);
        CHECK(c.discover().source == 2); // Failed return burned ID 1.
        observed = nullptr;
    }
    {
        Directory missing;
        Fake io;
        Runtime runtime(io);
        Coordinator c(runtime, [&] { return allocate_endpoint_session(missing.path, true); });
        CHECK(rejects([&] { c.prepare(1, config(), 1); }));
        CHECK(io.source && !monitor::valid(c.discover()));
        CHECK(access((missing.path + "/endpoint-sessions.log").c_str(), F_OK) != 0);
    }
    {
        Directory failed_prepare;
        provision_endpoint_sessions(failed_prepare.path, true);
        Fake io;
        Runtime runtime(io);
        Coordinator c(runtime,
                      [&] { return allocate_endpoint_session(failed_prepare.path, true); });
        io.cleanup_failure = 1;
        CHECK(rejects([&] { c.prepare(1, config(), 1); }));
        CHECK(io.source && !monitor::valid(c.discover()));
        io.cleanup_failure = 0;
        c.prepare(1, config(), 1);
        CHECK(c.discover().source == 2);
    }
    {
        Fake io;
        Runtime runtime(io);
        unsigned calls = 0;
        Coordinator c(runtime, [&] {
            ++calls;
            return 1;
        });
        io.halt_failure = true;
        CHECK(rejects([&] { c.prepare(1, config(), 1); }));
        CHECK(calls == 0);
        io.halt_failure = false;
        c.prepare(1, config(), 1);
        CHECK(c.discover().source == 1);
        CHECK(rejects([&] { c.prepare(1, config(), 1); })); // Broken allocator reuse detected.
        CHECK(io.source && !monitor::valid(c.discover()));
    }
    std::cout
        << "session integration PASS: inhibited reservation, exact discovery retry, restart, sync failure, no provisioning, burned IDs and HALT failure\n";
}
