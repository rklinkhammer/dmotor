#include "dmotor_sw/endpoint_session.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace dmotor_sw {
namespace recovery {
namespace {
constexpr const char* filename = "endpoint-sessions.log";
constexpr std::size_t record_size = 32;
constexpr uint64_t max_records = 1000000; // Bounded startup scan; no automatic compaction.
using Record = std::array<unsigned char, record_size>;
class Fd {
  public:
    explicit Fd(int value) : value_(value) {
        if (value < 0)
            throw std::runtime_error(std::string("Session store open: ") + std::strerror(errno));
    }
    ~Fd() {
        ::close(value_);
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const {
        return value_;
    }

  private:
    int value_;
};
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(unsigned char* p, uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i)
        p[i] = static_cast<unsigned char>(value >> (i * 8));
}
uint64_t get(const unsigned char* p, unsigned size) {
    uint64_t value = 0;
    for (unsigned i = 0; i < size; ++i)
        value |= uint64_t{p[i]} << (i * 8);
    return value;
}
uint32_t crc(const Record& r) {
    uint32_t value = UINT32_MAX;
    for (unsigned i = 0; i < 28; ++i) {
        value ^= r[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1U) ? 0xedb88320U : 0U);
    }
    return ~value;
}
Record encode(uint64_t id) {
    Record r{};
    std::memcpy(r.data(), "DMSID001", 8);
    put(r.data() + 8, id, 8);
    put(r.data() + 16, ~id, 8);
    put(r.data() + 28, crc(r), 4);
    return r;
}
void secure(int fd, bool directory) {
    struct stat st {};
    require(::fstat(fd, &st) == 0, "Cannot stat session store");
    require(directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode), "Wrong session store file type");
    require(st.st_uid == ::geteuid() && (st.st_mode & 07777) == (directory ? 0700 : 0600),
            "Session store requires owned private directory/file");
    require(directory || st.st_nlink == 1, "Session store must not have hard links");
}
void sync(int fd) {
    int result;
    do {
        result = ::fsync(fd);
    } while (result < 0 && errno == EINTR);
    require(result == 0, "Session store durability unknown; keep inhibited");
}
void write_record(int fd, const Record& r) {
    std::size_t used = 0;
    while (used < r.size()) {
        auto n = ::write(fd, r.data() + used, r.size() - used);
        if (n < 0 && errno == EINTR)
            continue;
        require(n > 0, "Session store write incomplete; keep inhibited");
        used += static_cast<std::size_t>(n);
    }
}
void lock(int fd) {
    int result;
    do {
        result = ::flock(fd, LOCK_EX);
    } while (result < 0 && errno == EINTR);
    require(result == 0, "Cannot lock session store");
}
} // namespace
void provision_endpoint_sessions(const std::string& directory, bool inhibited) {
    require(inhibited, "Session provisioning requires inhibition");
    Fd dir(::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    secure(dir.get(), true);
    Fd file(
        ::openat(dir.get(), filename, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    secure(file.get(), false);
    lock(file.get());
    // Failure leaves an unusable/uncertain file; never unlink and silently retry.
    write_record(file.get(), encode(0));
    sync(file.get());
    sync(dir.get());
}
uint64_t allocate_endpoint_session(const std::string& directory, bool inhibited) {
    require(inhibited, "Session allocation requires inhibition");
    Fd dir(::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    secure(dir.get(), true);
    Fd file(::openat(dir.get(), filename, O_RDWR | O_APPEND | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    secure(file.get(), false);
    lock(file.get());
    struct stat st {};
    require(::fstat(file.get(), &st) == 0, "Cannot size session store");
    require(st.st_size >= static_cast<off_t>(record_size) && st.st_size % record_size == 0,
            "Missing genesis or torn session record; maintenance required");
    const auto count = static_cast<uint64_t>(st.st_size) / record_size;
    require(count < max_records, "Session log exhausted; maintenance required");
    uint64_t previous = 0;
    for (uint64_t index = 0; index < count; ++index) {
        Record r{};
        std::size_t used = 0;
        while (used < r.size()) {
            auto n = ::read(file.get(), r.data() + used, r.size() - used);
            if (n < 0 && errno == EINTR)
                continue;
            require(n > 0, "Cannot read complete session record");
            used += static_cast<std::size_t>(n);
        }
        const auto id = get(r.data() + 8, 8);
        require(std::memcmp(r.data(), "DMSID001", 8) == 0 && get(r.data() + 16, 8) == ~id &&
                    get(r.data() + 24, 4) == 0 && get(r.data() + 28, 4) == crc(r),
                "Corrupt session record; maintenance required");
        require(index == 0 ? id == 0 : previous != UINT64_MAX && id == previous + 1,
                "Invalid session history; maintenance required");
        previous = id;
    }
    require(previous != UINT64_MAX, "Endpoint sessions exhausted; maintenance required");
    const auto next = previous + 1;
    write_record(file.get(), encode(next));
    sync(file.get());
    sync(dir.get());
    // A crash before this return may burn an ID; it must never make an issued
    // ID reusable. Parent-directory sync also covers provisioning crash races.
    return next;
}
} // namespace recovery
} // namespace dmotor_sw
