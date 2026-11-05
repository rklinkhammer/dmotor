#include "dmotor_sw/endpoint_session.hpp"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace dmotor_sw::recovery;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
// Link-time syscall seams belong only to this executable, never production code.
static int fault = 0, write_calls = 0, sync_calls = 0;
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_fsync(int);
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
    if (fault == 8)
        return 0;
    if (fault == 1) {
        if (write_calls++ == 0)
            return __real_write(fd, data, 7);
        errno = EIO;
        return -1;
    }
    if (fault == 4 && write_calls++ == 0) {
        errno = EINTR;
        return -1;
    }
    if (fault == 5 && size > 3)
        size = 3;
    return __real_write(fd, data, size);
}
extern "C" int __wrap_fsync(int fd) {
    if (fault == 6 && sync_calls++ == 1) {
        errno = EIO;
        return -1;
    }
    if (fault == 7 && sync_calls++ == 0) {
        errno = EINTR;
        return -1;
    }
    if (fault == 2) {
        errno = EIO;
        return -1;
    }
    if (fault == 3) {
        int result = __real_fsync(fd);
        if (result == 0)
            _exit(42);
        return result;
    }
    return __real_fsync(fd);
}
struct Store {
    std::string directory, file;
    Store() {
        char path[] = "/tmp/dmotor-session-test-XXXXXX";
        auto p = ::mkdtemp(path);
        CHECK(p);
        directory = p;
        file = directory + "/endpoint-sessions.log";
    }
    ~Store() {
        ::unlink(file.c_str());
        ::rmdir(directory.c_str());
    }
    void provision() {
        provision_endpoint_sessions(directory, true);
    }
    uint64_t allocate() {
        return allocate_endpoint_session(directory, true);
    }
};
template <class F> bool rejects(F f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}
int main() {
    {
        Store s;
        CHECK(rejects([&] { s.allocate(); })); // Never implicitly provision.
        CHECK(rejects([&] { provision_endpoint_sessions(s.directory, false); }));
        CHECK(::access(s.file.c_str(), F_OK) != 0);
        s.provision();
        // Fixed little-endian genesis/CRC vector, independently generated.
        const unsigned char golden[32] = {
            0x44, 0x4d, 0x53, 0x49, 0x44, 0x30, 0x30, 0x31, 0, 0, 0, 0, 0,    0,    0,    0,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0xb5, 0x1b, 0x62, 0x1a};
        unsigned char record[32];
        int genesis = ::open(s.file.c_str(), O_RDONLY);
        CHECK(genesis >= 0);
        CHECK(::read(genesis, record, 32) == 32);
        ::close(genesis);
        for (unsigned i = 0; i < 32; ++i)
            CHECK(record[i] == golden[i]);
        CHECK(rejects([&] { s.provision(); }));
        CHECK(rejects([&] { allocate_endpoint_session(s.directory, false); }));
        CHECK(s.allocate() == 1);
        CHECK(s.allocate() == 2);
        auto pid = ::fork();
        CHECK(pid >= 0);
        if (pid == 0)
            _exit(s.allocate() == 3 ? 0 : 1);
        int status = 0;
        CHECK(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(s.allocate() == 4); // Reopening in a new process preserves lifetime history.
    }
    {
        Store s;
        s.provision();
        constexpr unsigned n = 12;
        int pipefd[2];
        CHECK(::pipe(pipefd) == 0);
        pid_t children[n];
        for (unsigned i = 0; i < n; ++i) {
            children[i] = ::fork();
            CHECK(children[i] >= 0);
            if (children[i] == 0) {
                ::close(pipefd[0]);
                auto id = s.allocate();
                CHECK(::write(pipefd[1], &id, sizeof id) == sizeof id);
                _exit(0);
            }
        }
        ::close(pipefd[1]);
        std::set<uint64_t> ids;
        for (unsigned i = 0; i < n; ++i) {
            uint64_t id = 0;
            CHECK(::read(pipefd[0], &id, sizeof id) == sizeof id);
            ids.insert(id);
        }
        ::close(pipefd[0]);
        for (auto child : children) {
            int status = 0;
            CHECK(::waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                  WEXITSTATUS(status) == 0);
        }
        CHECK(ids.size() == n && *ids.begin() == 1 && *ids.rbegin() == n);
        CHECK(s.allocate() == n + 1);
    }
    for (int failure : {1, 2, 3, 6, 8}) {
        Store s;
        s.provision();
        if (failure == 3) {
            auto child = ::fork();
            CHECK(child >= 0);
            if (child == 0) {
                fault = 3;
                s.allocate();
                _exit(1);
            }
            int status = 0;
            CHECK(::waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                  WEXITSTATUS(status) == 42);
        } else {
            fault = failure;
            write_calls = 0;
            sync_calls = 0;
            CHECK(rejects([&] { s.allocate(); }));
            fault = 0;
        }
        if (failure == 1)
            CHECK(rejects([&] { s.allocate(); })); // Torn tail never falls back.
        else
            CHECK(s.allocate() ==
                  (failure == 8 ? 1U : 2U)); // Complete uncertain reservation is burned.
    }
    for (int mode : {4, 5, 7}) {
        Store s;
        s.provision();
        fault = mode;
        write_calls = 0;
        sync_calls = 0;
        CHECK(s.allocate() == 1);
        fault = 0;
        CHECK(s.allocate() == 2);
    }
    for (unsigned damage = 0; damage < 5; ++damage) {
        Store s;
        s.provision();
        s.allocate();
        s.allocate();
        int fd = ::open(s.file.c_str(), O_RDWR);
        CHECK(fd >= 0);
        if (damage < 3) {
            unsigned char b = 0xff;
            CHECK(::pwrite(fd, &b, 1, static_cast<off_t>(damage * 32 + 10)) == 1);
        }
        if (damage == 3)
            CHECK(::ftruncate(fd, 63) == 0);
        if (damage == 4)
            CHECK(::ftruncate(fd, 0) == 0);
        ::close(fd);
        CHECK(rejects([&] { s.allocate(); }));
    }
    {
        Store s;
        s.provision();
        CHECK(::chmod(s.file.c_str(), 0644) == 0);
        CHECK(rejects([&] { s.allocate(); }));
        CHECK(::chmod(s.file.c_str(), 0600) == 0);
        CHECK(::chmod(s.directory.c_str(), 0755) == 0);
        CHECK(rejects([&] { s.allocate(); }));
        CHECK(::chmod(s.directory.c_str(), 0700) == 0);
        std::string alias = s.directory + "/alias";
        CHECK(::link(s.file.c_str(), alias.c_str()) == 0);
        CHECK(rejects([&] { s.allocate(); }));
        ::unlink(alias.c_str());
        ::unlink(s.file.c_str());
        CHECK(::symlink("/dev/null", s.file.c_str()) == 0);
        CHECK(rejects([&] { s.allocate(); }));
    }
    {
        Store s;
        s.provision();
        int fd = ::open(s.file.c_str(), O_RDWR);
        CHECK(fd >= 0);
        CHECK(::ftruncate(fd, 32000000) == 0);
        ::close(fd);
        CHECK(rejects([&] { s.allocate(); }));
    }
    std::cout
        << "endpoint session PASS: reopen, concurrency, short writes, EINTR, torn/corrupt stores, uncertain sync, crash after sync, permissions and exhaustion\n";
}
