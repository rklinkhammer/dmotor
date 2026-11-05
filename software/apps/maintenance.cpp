#include "dmotor_sw/client.hpp"
#include "dmotor_sw/version.hpp"
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include <csignal>
#include <chrono>
#include <functional>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dmotor_sw;
namespace {
volatile std::sig_atomic_t exiting = 0;
void stop(int) {
    exiting = 1;
}
struct Terminal {
    termios original{};
    bool terminal = false;
    ~Terminal() {
        if (terminal)
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
    }
};
int32_t rpm(std::string s) {
    bool negative = false;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
        negative = s[0] == '-';
        s.erase(0, 1);
    }
    const auto dot = s.find('.');
    std::string whole = s.substr(0, dot),
                fraction = dot == std::string::npos ? "" : s.substr(dot + 1);
    if (whole.empty() || whole.size() > 3 ||
        whole.find_first_not_of("0123456789") != std::string::npos || fraction.size() > 3 ||
        fraction.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Use RPM with at most three decimal places");
    while (fraction.size() < 3)
        fraction += '0';
    const auto value = std::stoi(whole) * 1000 + std::stoi(fraction);
    if (value > 400000)
        throw std::runtime_error("400 RPM capability exceeded");
    return negative ? -value : value;
}
void menu() {
    std::cout
        << "\nDMOTOR maintenance (exclusive commissioning; mode selected at daemon startup)\n"
           "m MASK  select motors (decimal bitmask 1..15; selection does not move motors)\n"
           "s       status and encoder positions\n"
           "r       explicit global reset/configure/recovery; resets position epochs\n"
           "e       enable selected motors (wait for ready before velocity)\n"
           "v R0 R1 R2 R3  selected-motor RPM targets; unused entries ignored\n"
           "t       controlled STOP selected motors; global rearm needed afterward\n"
           "d       disable selected motors\n"
           "!       immediate HALT ALL, including during partial input\n"
           "q       HALT ALL and quit\n"
           "p PERCENT signed duty, e.g. p 25 or p -12.5 (direct-duty daemon only)\n"
           "Powered daemon: 1-second pulse; updates do not extend it; zero/expiry require rearm.\n"
           "x       inject target timeout (inhibited commissioning only)\n"
           "w       inject watchdog timeout (inhibited commissioning only)\n"
           "Configuration is supplied to dmotord; no live retuning. Position/dwell unavailable.\n> "
        << std::flush;
}
} // namespace
int main(int argc, char** argv) {
    if (startup_version("dmotor-maint", argc, argv))
        return 0;
    if (argc != 1) {
        std::cerr << "Usage: dmotor-maint\n";
        return 2;
    }
    try {
        Terminal c;
#ifdef DMOTOR_TEST_SERVICE
        const char* test_directory = std::getenv("DMOTOR_TEST_DIRECTORY");
        if (!test_directory || !*test_directory)
            throw std::runtime_error("Missing DMOTOR_TEST_DIRECTORY");
        const std::string socket_name = std::string(test_directory) + "/control.sock";
#else
        const std::string socket_name = "/run/dmotor/control.sock";
#endif
        Client client(socket_name);
        if (isatty(STDIN_FILENO)) {
            if (tcgetattr(STDIN_FILENO, &c.original))
                throw std::runtime_error("Cannot read terminal mode");
            auto raw = c.original;
            raw.c_lflag &= static_cast<tcflag_t>(~ICANON);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSANOW, &raw))
                throw std::runtime_error("Cannot set terminal mode");
            c.terminal = true;
        }
        if (std::signal(SIGINT, stop) == SIG_ERR || std::signal(SIGTERM, stop) == SIG_ERR ||
            std::signal(SIGHUP, stop) == SIG_ERR)
            throw std::runtime_error("Cannot install exit handlers");
        uint32_t mask = 1;
        std::string line;
        menu();
        auto ping = std::chrono::steady_clock::now();
        while (!exiting) {
            pollfd input{STDIN_FILENO, POLLIN, 0};
            poll(&input, 1, static_cast<int>(Client::recommended_ping_ms));
            if (exiting)
                break;
            if (input.revents & (POLLIN | POLLHUP)) {
                char ch = 0;
                if (read(STDIN_FILENO, &ch, 1) != 1) {
                    exiting = 1;
                    break;
                }
                if (ch == '!') {
                    const auto reply = client.halt();
                    line.clear();
                    std::cout << (reply.okay ? "\nHALT confirmed\n" : "\nHALT failed\n") << "> "
                              << std::flush;
                } else if (ch == '\n') {
                    try {
                        std::istringstream in(line);
                        char op = 0;
                        in >> op;
                        std::function<ClientResult()> command;
                        if (op == 'm') {
                            uint32_t selected = 0;
                            std::string extra;
                            if (!(in >> selected) || !selected || selected > 15 || in >> extra)
                                throw std::runtime_error("Invalid mask");
                            mask = selected;
                            std::cout << "Selected mask " << mask << '\n';
                        } else if (op) {
                            switch (op) {
                                case 's':
                                    command = [&] { return client.status(); };
                                    break;
                                case 'r':
                                    command = [&] { return client.prepare(); };
                                    break;
                                case 'p': {
                                    std::string value;
                                    if (!(in >> value))
                                        throw std::runtime_error("Duty percentage required");
                                    const auto milli = rpm(value);
                                    if (milli > 100000 || milli < -100000)
                                        throw std::runtime_error("Duty exceeds 100 percent");
                                    const auto duty = static_cast<int32_t>(
                                        (int64_t{milli} * 65536 + (milli >= 0 ? 50000 : -50000)) /
                                        100000);
                                    command = [&, duty] { return client.duty(duty); };
                                    break;
                                }
                                case 'x':
                                    command = [&] { return client.inject_target_timeout(); };
                                    break;
                                case 'w':
                                    command = [&] { return client.inject_watchdog_timeout(); };
                                    break;
                                case 'e':
                                    command = [&] { return client.enable(mask); };
                                    break;
                                case 't':
                                    command = [&] { return client.stop(mask); };
                                    break;
                                case 'd':
                                    command = [&] { return client.disable(mask); };
                                    break;
                                case 'q':
                                    exiting = 1;
                                    break;
                                case 'v': {
                                    std::array<int32_t, 4> targets{};
                                    for (auto& v : targets) {
                                        std::string value;
                                        if (!(in >> value))
                                            throw std::runtime_error("Four RPM values required");
                                        v = rpm(value);
                                    }
                                    command = [&, targets] {
                                        return client.velocity(mask, targets);
                                    };
                                    break;
                                }
                                default:
                                    throw std::runtime_error("Unknown menu command");
                            }
                            std::string extra;
                            if (in >> extra)
                                throw std::runtime_error("Unexpected input");
                            if (!exiting) {
                                auto reply = command();
                                std::cout << (reply.okay ? "Accepted" : "Rejected; HALT requested")
                                          << " status=0x" << std::hex << reply.status << std::dec
                                          << " applied=" << reply.applied << " " << reply.message
                                          << '\n';
                                if (op == 's')
                                    for (unsigned m = 0; m < 4; ++m) {
                                        std::cout << "motor " << m
                                                  << " position=" << reply.position_counts[m]
                                                  << " measured_mrpm=" << reply.measured_mrpm[m]
                                                  << '\n';
                                    }
                            }
                        }
                    } catch (const ClientError&) {
                        throw;
                    } catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                    }
                    line.clear();
                    std::cout << "> " << std::flush;
                } else if (ch == 127 || ch == '\b') {
                    if (!line.empty())
                        line.pop_back();
                } else if (line.size() < 256)
                    line += ch;
                else
                    throw std::runtime_error("Input line too long");
            }
            const auto now = std::chrono::steady_clock::now();
            if (now - ping >= std::chrono::milliseconds(Client::recommended_ping_ms)) {
                if (!client.ping().okay)
                    throw ClientError("Lease renewal rejected");
                ping = now;
            }
        }
        if (!client.quit().okay)
            throw std::runtime_error("HALT not confirmed");
        std::cout << "HALT ALL confirmed; maintenance closed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
