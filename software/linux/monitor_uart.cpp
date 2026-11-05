#include "dmotor_sw/monitor_uart.hpp"
#include "dmotor_sw/device.hpp"
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
namespace dmotor_sw {
namespace monitor {
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw IoError(message);
}
} // namespace
Uart::Uart(const std::string& path) {
    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    require(fd_ >= 0, "Cannot open monitor UART");
    bool exclusive = false;
    try {
        require(flock(fd_, LOCK_EX | LOCK_NB) == 0, "Monitor UART already owned");
        require(ioctl(fd_, TIOCEXCL) == 0, "Cannot exclusively claim monitor UART");
        exclusive = true;
        termios t{};
        require(tcgetattr(fd_, &t) == 0, "Monitor UART is not a tty");
        cfmakeraw(&t);
        t.c_cflag &= ~(CSTOPB | PARENB | CRTSCTS | CSIZE);
        t.c_cflag |= CS8 | CLOCAL | CREAD;
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        require(cfsetispeed(&t, B115200) == 0 && cfsetospeed(&t, B115200) == 0 &&
                    tcsetattr(fd_, TCSANOW, &t) == 0,
                "Cannot configure monitor UART 115200 8N1");
        require(tcflush(fd_, TCIOFLUSH) == 0, "Cannot discard stale UART bytes");
    } catch (...) {
        if (exclusive)
            ioctl(fd_, TIOCNXCL);
        ::close(fd_);
        fd_ = -1;
        throw;
    }
}
Uart::~Uart() {
    if (fd_ >= 0) {
        ioctl(fd_, TIOCNXCL);
        ::close(fd_);
    }
}
void Uart::queue(const Frame& f, uint64_t deadline) {
    require(!transmitting(), "Monitor transmit slot busy");
    require(deadline != 0, "Monitor transmit deadline required");
    size_ = encode(f, tx_);
    sent_ = 0;
    require(size_ != 0, "Invalid monitor frame");
    deadline_ = deadline;
}
bool Uart::poll(uint64_t now, Frame& received) {
    pollfd descriptor{fd_, POLLIN, 0};
    const auto ready = ::poll(&descriptor, 1, 0);
    require(ready >= 0 || errno == EINTR, "Monitor UART poll failed");
    require(!(descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)), "Monitor UART disconnected");
    if (transmitting()) {
        require(now < deadline_, "Monitor transmit timeout");
        const auto n = ::write(fd_, tx_.data() + sent_, size_ - sent_);
        if (n > 0)
            sent_ += static_cast<std::size_t>(n);
        else
            require(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR),
                    "Monitor UART write failed");
    }
    // Bounded work, including hostile traffic. Stop exactly at a frame boundary;
    // never consume bytes from the next frame without retaining them.
    for (std::size_t i = 0; i < max_encoded; ++i) {
        uint8_t b = 0;
        const auto n = ::read(fd_, &b, 1);
        if (n == 0)
            return false;
        if (n < 0) {
            require(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR,
                    "Monitor UART read failed");
            return false;
        }
        const auto result = decoder_.push(b, received);
        if (result == Decode::frame)
            return true;
        if (result == Decode::rejected)
            ++rejected_;
    }
    return false;
}
} // namespace monitor
} // namespace dmotor_sw
