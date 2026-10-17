#include "dmotor_sw/linux_inspection.hpp"
#include "dmotor/controller_registers.hpp"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
namespace dmotor_sw {
namespace {
namespace reg = dmotor::controller_reg;
constexpr size_t required_bytes = reg::TELEMETRY_BASE + 4 * reg::TELEMETRY_STRIDE;
void barrier() {
#if defined(__arm__) || defined(__aarch64__)
    asm volatile("dsb sy" ::: "memory");
#else
    // Native regular-file fixture only; open_uio rejects non-ARM hosts.
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}
std::string attribute(const std::string& path) {
    std::ifstream stream(path); std::string text;
    if (!(stream >> text)) throw IoError("Cannot read UIO attribute: " + path);
    std::string extra; if (stream >> extra) throw IoError("Malformed UIO attribute: " + path);
    return text;
}
size_t number(const std::string& text) {
    if (text.empty() || text[0] == '-') throw IoError("Invalid UIO map number");
    size_t end = 0;
    try {
        const auto value = std::stoull(text, &end, 0);
        if (end != text.size() || value > std::numeric_limits<size_t>::max()) throw IoError("Invalid UIO map size");
        return static_cast<size_t>(value);
    } catch (const std::logic_error&) { throw IoError("Invalid UIO map number"); }
}
}
std::unique_ptr<LinuxInspection> LinuxInspection::open_uio(unsigned index, Mode mode) {
#if !defined(__arm__) && !defined(__aarch64__)
    (void)index;
    throw IoError("Hardware UIO inspection requires an ARM Linux target");
#endif
    const std::string name = "uio" + std::to_string(index);
    const std::string sys = "/sys/class/uio/" + name;
    if (attribute(sys + "/name") != "dmotor-inspection") throw IoError("Unexpected UIO device name");
    const size_t size = number(attribute(sys + "/maps/map0/size"));
    const size_t offset = number(attribute(sys + "/maps/map0/offset"));
    const std::string expected_device = attribute(sys + "/dev");
    int fd = ::open(("/dev/" + name).c_str(), O_RDWR | O_SYNC | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) throw IoError("Cannot open " + name + ": " + std::strerror(errno));
    struct stat status{};
    if (fstat(fd, &status) != 0 || !S_ISCHR(status.st_mode) ||
        std::to_string(major(status.st_rdev)) + ":" + std::to_string(minor(status.st_rdev)) != expected_device) {
        close(fd); throw IoError("UIO device node does not match sysfs");
    }
    // Constructor consumes fd on both success and failure.
    return std::unique_ptr<LinuxInspection>(new LinuxInspection(fd, size, offset, mode));
}
LinuxInspection::LinuxInspection(int fd, size_t size, size_t offset, Mode mode)
    : mode_(mode), fd_(fd), size_(size), offset_(offset) {
    try {
        const long page = sysconf(_SC_PAGESIZE);
        if (fd < 0 || page <= 0 || !size || size > 1024 * 1024 ||
            offset >= static_cast<size_t>(page) || offset % 4 || offset > size ||
            size - offset < required_bytes) throw IoError("Invalid inspection mapping geometry");
        if (flock(fd_, LOCK_EX | LOCK_NB) != 0) throw IoError("Controller already owned or locking unavailable");
        struct stat status{};
        if (fstat(fd_, &status) != 0) throw IoError("Cannot stat mapping");
        if (S_ISREG(status.st_mode) && (status.st_size < 0 || static_cast<uint64_t>(status.st_size) < size))
            throw IoError("Truncated mapping fixture");
        mapping_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapping_ == MAP_FAILED) { mapping_ = nullptr; throw IoError("Cannot map controller"); }
        if (load(reg::IDENTITY) != reg::PROFILE_ID) throw IoError("Unsupported controller identity");
        require_disabled();
    } catch (...) { cleanup(); throw; }
}
LinuxInspection::~LinuxInspection() { cleanup(); }
void LinuxInspection::cleanup() {
    if (mapping_) { munmap(mapping_, size_); mapping_ = nullptr; }
    // close releases flock; explicit LOCK_UN could release an inherited lock.
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
}
uint32_t LinuxInspection::load(uint32_t offset) const {
    barrier();
    const auto* address = reinterpret_cast<volatile const uint32_t*>(
        static_cast<const char*>(mapping_) + offset_ + offset);
    const uint32_t value = *address;
    barrier();
    return value;
}
void LinuxInspection::require_disabled() const {
    if (load(reg::STATUS) & (mode_ == Mode::encoder ? 255U : 15U)) throw IoError("Access requires all motors disabled (and unconfigured for encoder mode)");
}
uint32_t LinuxInspection::read32(uint32_t offset) {
    const bool local = offset == reg::IDENTITY || offset == reg::STATUS || offset == reg::SESSION || offset == reg::SNAPSHOT;
    const bool telemetry = offset >= reg::TELEMETRY_BASE && offset < required_bytes;
    if (offset % 4 || (!local && !telemetry)) throw IoError("Read outside inspection allowlist");
    require_disabled();
    const uint32_t value = load(offset);
    require_disabled();
    return value;
}
void LinuxInspection::write32(uint32_t offset, uint32_t value) {
    const bool snapshot = offset == reg::SNAPSHOT && value == 1;
    const bool encoder = mode_ == Mode::encoder &&
        ((offset == reg::SESSION && value != 0) || (offset == reg::CORE_RESET && value <= 1));
    if (!snapshot && !encoder) throw IoError("Write outside bring-up allowlist");
    require_disabled(); barrier();
    *reinterpret_cast<volatile uint32_t*>(static_cast<char*>(mapping_) + offset_ + offset) = value;
    fence();
}
void LinuxInspection::fence() {
    // Same-device readback following DSB; platform ordering still needs board evidence.
    barrier();
    if (load(reg::IDENTITY) != reg::PROFILE_ID) throw IoError("Controller identity changed");
    require_disabled(); barrier();
}
}
