#include "dmotor/velocity.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static uint64_t now, count, edge, age;
static uint32_t direction_epoch, invalid, status;
static int32_t speed;
static bool negative;
static void call(bool reset = false, uint32_t generation = 1) {
    dmotor_velocity(reset, now, count, edge, direction_epoch, invalid, negative, generation, 10296,
                    5, 10000000, speed, age, status);
}
static void init(bool reverse = false) {
    now = 0;
    count = 0;
    edge = 0;
    direction_epoch = 0;
    invalid = 0;
    negative = reverse;
    call(true);
    call();
}
int main() {
    // Integer fabric timestamps generated from the exact rational nominal CPR.
    for (const int rpm : {1, 5, 30, 130}) {
        for (const bool reverse : {false, true}) {
            init(reverse);
            uint64_t next = 1;
            unsigned assessed = 0;
            for (unsigned tick = 1; tick <= 400; ++tick) {
                now = static_cast<uint64_t>(tick) * 100000;
                while ((next * 6000000000ULL * 5) / (static_cast<uint64_t>(rpm) * 10296) <= now) {
                    edge = (next * 6000000000ULL * 5) / (static_cast<uint64_t>(rpm) * 10296);
                    ++count;
                    ++next;
                }
                call();
                CHECK((status >> 8) == 0);
                if (tick >= 100) {
                    CHECK((status & 3) == 1);
                    const int32_t target = (reverse ? -1 : 1) * rpm * 1000;
                    CHECK(std::abs(speed - target) <= rpm * 20);
                    ++assessed;
                }
            }
            CHECK(assessed == 301);
        }
    }
    init();
    for (unsigned tick = 1; tick <= 99; ++tick) {
        now += 100000;
        call();
        CHECK(!(status & 1));
    }
    now += 100000;
    call();
    CHECK((status & 3) == 3 && speed == 0 && age == 10000000);
    // First edge after timed-out zero invalidates the zero; second qualifies speed.
    now += 100000;
    edge = now;
    ++count;
    call();
    CHECK(!(status & 1));
    now += 100000;
    edge = now;
    ++count;
    call();
    CHECK((status & 1) && speed > 0);
    ++direction_epoch;
    negative = true;
    now += 100000;
    edge = now;
    ++count;
    call();
    CHECK(!(status & 1));
    now += 100000;
    edge = now;
    ++count;
    call();
    CHECK((status & 1) && speed < 0);
    // Even a reversal and return between snapshots invalidates the interval.
    direction_epoch += 2;
    now += 100000;
    edge = now;
    count += 2;
    call();
    CHECK(!(status & 1));
    now += 100000;
    edge = now;
    ++count;
    call();
    CHECK(status & 1);
    ++invalid;
    now += 100000;
    call();
    CHECK(!(status & 1));
    now += 100000;
    edge = now;
    ++count;
    call();
    CHECK(!(status & 1));
    now += 100000;
    edge = now;
    ++count;
    call();
    CHECK(status & 1);
    // A configuration change requires an explicit estimator reset.
    now += 100000;
    call(false, 2);
    CHECK((status >> 8) & 2);
    CHECK(!(status & 1));
    init();
    now = 200000;
    call();
    CHECK((status >> 8) & 2);
    init();
    now = 100000;
    count = 101;
    edge = now;
    call();
    CHECK((status >> 8) & 4);
    init();
    now = 100000;
    count = 1;
    edge = now + 1;
    call();
    CHECK((status >> 8) & 4);
    std::cout << "velocity: both-direction 1/5/30/130 RPM sweep, zero timeout, restart, reversal, "
                 "invalid edges and snapshot faults passed\n";
}
