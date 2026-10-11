#include "timing_fixture.hpp"
#include "dmotor/configuration.hpp"
#include "dmotor/registers.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
int main() {
    dmotor::Configuration c;
    using namespace dmotor::reg;
    uint32_t read = 0, action = 0, arg = 0;
    auto write = [&](uint32_t address, uint32_t value, uint32_t state = 0, uint32_t ring = 0,
                     uint32_t producer = 0, uint32_t consumer = 0, uint32_t faults = 0,
                     uint32_t live = 0) {
        return c.access(true, address, value, state, faults, live, 1, ring, producer, consumer, 5,
                        read, action, arg);
    };
    CHECK(!write(CONFIG_COMMIT, 1));
    CHECK(!c.valid());
    CHECK(write(WATCHDOG_US, 999));
    CHECK(!write(CONFIG_COMMIT, 1));
    CHECK(write(WATCHDOG_US, 10000001));
    CHECK(!write(CONFIG_COMMIT, 1));
    CHECK(write(WATCHDOG_US, 1000));
    CHECK(write(MAX_SPEED_MRPM, 999));
    CHECK(!write(CONFIG_COMMIT, 1));
    CHECK(write(MAX_SPEED_MRPM, 130000));
    CHECK(!write(CONFIG_COMMIT, 1, 1));
    CHECK(!write(CONFIG_COMMIT, 1, 0, 2));
    CHECK(!write(CONFIG_COMMIT, 1, 0, 0, 1, 0));
    CHECK(write(CONFIG_COMMIT, 1));
    CHECK(c.generation() == 1 && c.timeout_cycles() == test_timing::cycles_us(1000));
    CHECK(!write(ENABLE, 1, 0, 2, 0, 0, 8, 0));
    CHECK(write(ENABLE, 1, 0, 2));
    CHECK(action == 1);
    CHECK(!write(ACK_LOCAL, 8, 0, 0, 0, 0, 8, 8));
    CHECK(!write(ACK_LOCAL, 16, 0, 0, 0, 0, 8, 0));
    CHECK(write(ACK_LOCAL, 8, 0, 0, 0, 0, 8, 0));
    CHECK(action == 3 && arg == 8);
    CHECK(!write(ACK_EXTERNAL, 1, 4));
    CHECK(write(ACK_EXTERNAL, 1, 2));
    CHECK(write(REFRESH_SESSION, 4));
    CHECK(!write(REFRESH_SEQUENCE, 1, 1));
    CHECK(write(REFRESH_SESSION, 5));
    CHECK(write(REFRESH_SEQUENCE, 1, 1));
    CHECK(action == 2 && arg == 1);
    CHECK(!write(REFRESH_SEQUENCE, 1, 0));
    std::cout
        << "configuration: bounds, atomic commit, authorization and acknowledgment guards passed\n";
}
