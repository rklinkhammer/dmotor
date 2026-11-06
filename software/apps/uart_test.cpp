#include "dmotor_diag/uart.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <termios.h>
#include <unistd.h>
using namespace dmotor_diag;
namespace {
uint64_t millis() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}
void require(bool ok, const char* reason) {
    if (!ok)
        throw std::runtime_error(reason);
}
struct Port {
    int fd = -1;
    bool exclusive = false;
    ~Port() {
        if (fd >= 0) {
            if (exclusive)
                ioctl(fd, TIOCNXCL);
            close(fd);
        }
    }
};
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout
            << "dmotor-uart-test DEVICE [COUNT=100, 1..1000]\nDiagnostic only; motor drive power disconnected, ESP32 bench image required.\n";
        return 0;
    }
    unsigned sent = 0, verified = 0, rejected = 0, mismatched = 0;
    uint64_t run = 0, started = millis();
    try {
        require(argc == 2 || argc == 3, "Usage: dmotor-uart-test DEVICE [COUNT]");
        unsigned count = 100;
        if (argc == 3) {
            std::size_t end = 0;
            const auto n = std::stoul(argv[2], &end);
            require(end == std::string(argv[2]).size() && n >= 1 && n <= 1000,
                    "Count must be 1..1000");
            count = static_cast<unsigned>(n);
        }
        require(getrandom(&run, sizeof(run), 0) == sizeof(run) && run,
                "Cannot create run identifier");
        Port port;
        port.fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
        require(port.fd >= 0, "Cannot open UART");
        require(flock(port.fd, LOCK_EX | LOCK_NB) == 0 && ioctl(port.fd, TIOCEXCL) == 0,
                "UART already owned or exclusive access unavailable");
        port.exclusive = true;
        termios t{};
        require(tcgetattr(port.fd, &t) == 0, "Not a tty");
        cfmakeraw(&t);
        t.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
        t.c_cflag |= CS8 | CLOCAL | CREAD;
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        require(cfsetispeed(&t, B115200) == 0 && cfsetospeed(&t, B115200) == 0 &&
                    tcsetattr(port.fd, TCSANOW, &t) == 0 && tcflush(port.fd, TCIOFLUSH) == 0,
                "UART setup failed");
        std::cout << "DIAGNOSTIC v1 115200 8N1 run=" << run << " count=" << count << std::endl;
        Decoder decoder;
        for (unsigned seq = 1; seq <= count; ++seq) {
            const auto deadline = millis() + 1000;
            std::array<uint8_t, max_wire> bytes{};
            const auto size = encode(make(1, run, seq), bytes);
            std::size_t offset = 0;
            bool done = false;
            while (!done) {
                require(millis() < deadline && millis() - started < 120000, "Response/TX timeout");
                pollfd p{port.fd, static_cast<short>(POLLIN | (offset < size ? POLLOUT : 0)), 0};
                const auto result = poll(&p, 1, 10);
                require(result >= 0 || errno == EINTR, "Poll failed");
                require(!(p.revents & (POLLERR | POLLHUP | POLLNVAL)), "UART disconnected");
                if (offset < size && (p.revents & POLLOUT)) {
                    const auto n = write(port.fd, bytes.data() + offset, size - offset);
                    if (n > 0) {
                        offset += static_cast<std::size_t>(n);
                        if (offset == size)
                            ++sent;
                    } else
                        require(n < 0 && (errno == EAGAIN || errno == EINTR), "Write failed");
                }
                uint8_t input[256];
                const auto n = read(port.fd, input, sizeof(input));
                require(n >= 0 || errno == EAGAIN || errno == EINTR, "Read failed");
                for (ssize_t i = 0; i < n; ++i) {
                    Packet reply;
                    const auto status = decoder.push(input[i], reply);
                    if (status == Decode::rejected)
                        ++rejected;
                    if (status == Decode::packet) {
                        if (done || offset != size || reply.kind != 2 || reply.run != run ||
                            reply.sequence != seq || !pattern(reply))
                            ++mismatched;
                        else {
                            done = true;
                            ++verified;
                        }
                    }
                }
            }
        }
        require(!rejected && !mismatched, "Corrupt, stale, duplicate or unexpected packets");
        std::cout << "PASS ";
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << " sent=" << sent << " verified=" << verified
                  << " rejected=" << rejected << " mismatched=" << mismatched << " run=" << run
                  << '\n';
        return 1;
    }
    std::cout << "sent=" << sent << " verified=" << verified << " rejected=" << rejected
              << " mismatched=" << mismatched << " elapsed_ms=" << millis() - started << '\n';
    return 0;
}
