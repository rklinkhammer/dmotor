#include "dmotor_diag/gpio.hpp"
#include <stdexcept>
#include <iostream>
using namespace dmotor_diag;
void check(bool value) {
    if (!value)
        throw std::runtime_error("U1 assertion failed");
}
int main() {
    BenchConsole parser;
    auto line = [&](const char* s) {
        BenchCommand result = BenchCommand::none;
        while (*s) {
            result = parser.push(*s++);
        }
        return result;
    };
    check(line("hb10\n") == BenchCommand::hb10);
    check(line("gpio\r") == BenchCommand::gpio);
    check(line("\n") == BenchCommand::none);
    check(line("stop\n") == BenchCommand::stop);
    check(line("quiet\n") == BenchCommand::quiet);
    check(line("help\n") == BenchCommand::help);
    check(line("hhhhhhhhhhhhhhhhhb10\n") == BenchCommand::invalid);
    check(line("hb10\001\n") == BenchCommand::invalid);
    check(line("hb100\n") == BenchCommand::invalid);
    check(line("hb10\n") == BenchCommand::hb10);
    BenchHeartbeat hb;
    check(!hb.active() && !hb.high());
    check(!hb.start(0, 0, false));
    check(!hb.start(100, 99, true));
    check(!hb.start(0, 10000, true));
    check(hb.start(0, 0, true));
    hb.step(9999, true);
    check(!hb.high());
    hb.step(10000, true);
    check(hb.high());
    check(!hb.start(10000, 10000, true));
    check(hb.remaining(10000) == 9990000);
    hb.step(20000, true);
    check(!hb.high());
    hb.step(40000, true);
    check(!hb.active() && !hb.high() && hb.reason() == HbEnd::late);
    check(hb.start(50000, 50000, true));
    hb.step(60000, true);
    check(hb.high());
    hb.step(60001, false);
    check(!hb.high() && hb.reason() == HbEnd::disconnected);
    hb.step(70000, true);
    check(!hb.active()); // Reconnect never restarts.
    check(hb.start(80000, 80000, true));
    hb.step(90000, true);
    hb.stop();
    check(!hb.high() && !hb.active());
    check(hb.start(100000, 100000, true));
    unsigned edges = 0;
    bool level = false;
    for (uint64_t now = 100000; now <= 10100000; now += 1000) {
        hb.step(now, true);
        if (hb.high() != level) {
            ++edges;
            level = hb.high();
        }
    }
    check(edges == 1000 && !hb.high() && !hb.active() && hb.reason() == HbEnd::expired);
    check(hb.remaining(10100000) == 0);
    std::cout
        << "U1 parser, default off, repeated/stale start, stop, expiry, disconnect and missed-edge tests PASS\n";
}
