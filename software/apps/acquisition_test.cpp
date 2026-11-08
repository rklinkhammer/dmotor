#include "dmotor_diag/acquisition.hpp"
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
            << "dmotor-acquisition-test DEVICE [COUNT=100, 1..1000] [--synthetic]\nDiagnostic only; motor drive power disconnected, ESP32 pre-PCB acquisition image required; no daemon or motor control.\n";
        return 0;
    }
    unsigned sent = 0, verified = 0, rejected = 0, mismatched = 0;
    uint64_t run = 0, started = millis();
    try {
        require(argc >= 2 && argc <= 4, "Usage: dmotor-acquisition-test DEVICE [COUNT]");
        unsigned count = 100;
        const bool synthetic=argc==4;
        require(!synthetic || std::string(argv[3])=="--synthetic", "Expected --synthetic");
        if (argc >= 3) {
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
        std::cout << "ACQUISITION DIAGNOSTIC v1 (not health-qualified) 115200 8N1 run=" << run << " synthetic=" << synthetic << " count=" << count << std::endl;
        Decoder decoder(dmotor_acq_diag::magic);
        dmotor_acq_diag::Tracker tracker;
        for (unsigned seq = 1; seq <= count; ++seq) {
            usleep(50000); // Sample at <=20 Hz; each response must advance acquisition.
            const auto requested = millis();
            const auto deadline = requested + 1000;
            std::array<uint8_t, max_wire> bytes{};
            const auto size = encode(dmotor_acq_diag::request(run, seq), bytes, dmotor_acq_diag::magic);
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
                        const auto why = (done || offset != size)
                            ? dmotor_acq_diag::Rejection::sequence
                            : tracker.check(reply, run, seq, (millis()-requested)*1000, synthetic);
                        if (why != dmotor_acq_diag::Rejection::none) {
                            ++mismatched;
                            throw std::runtime_error(std::string("Sample rejected: ") + dmotor_acq_diag::reason(why));
                        } else {
                            done = true;
                            ++verified;
                            const auto sample=dmotor_acq_diag::unpack(reply);
                            std::cout << "sample cycle=" << sample.cycle << " duration_us=" << sample.duration
                                      << " remote_age_us=" << sample.age << " address=" << unsigned(sample.address)
                                      << " mask=" << sample.raw[0] << " shunt=" << sample.raw[1]
                                      << " bus=" << sample.raw[2] << " adc0=" << sample.adc0
                                      << " adc1=" << sample.adc1 << '\n';
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
