#include "timing_fixture.hpp"
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
                    5, test_timing::cycles_us(100000), speed, age, status);
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
                now = static_cast<uint64_t>(tick) * test_timing::tick_cycles;
                while ((next * (60ULL * dmotor::timing::fabric_hz) * 5) / (static_cast<uint64_t>(rpm) * 10296) <= now) {
                    edge = (next * (60ULL * dmotor::timing::fabric_hz) * 5) / (static_cast<uint64_t>(rpm) * 10296);
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
        now += test_timing::tick_cycles;
        call();
        CHECK(!(status & 1));
    }
    now += test_timing::tick_cycles;
    call();
    CHECK((status & 3) == 3 && speed == 0 && age == test_timing::cycles_us(100000));
    // First edge after timed-out zero invalidates the zero; second qualifies speed.
    now += test_timing::tick_cycles;
    edge = now;
    ++count;
    call();
    CHECK(!(status & 1));
    now += test_timing::tick_cycles;
    edge = now;
    ++count;
    call();
    CHECK((status & 1) && speed > 0);
    ++direction_epoch;
    negative = true;
    now += test_timing::tick_cycles;
    edge = now;
    ++count;
    call();
    CHECK(!(status & 1));
    now += test_timing::tick_cycles;
    edge = now;
    ++count;
    call();
    CHECK((status & 1) && speed < 0);
    // Even a reversal and return between snapshots invalidates the interval.
    direction_epoch += 2;
    now += test_timing::tick_cycles;
    edge = now;
    count += 2;
    call();
    CHECK(!(status & 1));
    now += test_timing::tick_cycles;
    edge = now;
    ++count;
    call();
    CHECK(status & 1);
    ++invalid;
    now += test_timing::tick_cycles;
    call();
    CHECK(!(status & 1));
    now += test_timing::tick_cycles;
    edge = now;
    ++count;
    call();
    CHECK(!(status & 1));
    now += test_timing::tick_cycles;
    edge = now;
    ++count;
    call();
    CHECK(status & 1);
    // A configuration change requires an explicit estimator reset.
    now += test_timing::tick_cycles;
    call(false, 2);
    CHECK((status >> 8) & 2);
    CHECK(!(status & 1));
    init();
    now = (2 * test_timing::tick_cycles);
    call();
    CHECK((status >> 8) & 2);
    init();
    now = test_timing::tick_cycles;
    count = 101;
    edge = now;
    call();
    CHECK((status >> 8) & 4);
    init();
    now = test_timing::tick_cycles;
    count = 1;
    edge = now + 1;
    call();
    CHECK((status >> 8) & 4);
    // Full configured scaling bounds, maximum accepted events per snapshot,
    // both signs, and the original wide rational expression as an oracle.
    for (uint32_t num : {1U, 1560U, 10296U, 999999U, 1000000U}) {
        for (uint32_t den : {1U, 5U, 9999U, 10000U}) {
            for (uint32_t events : {1U, 10U, 11U, 13U, 14U, 99U, 100U}) {
                for (bool reverse : {false, true}) {
                    dmotor_velocity(true, 0, 0, 0, 0, 0, reverse, 1, num, den,
                                    dmotor::timing::zero_timeout_max_cycles, speed, age, status);
                    dmotor_velocity(false, 0, 0, 0, 0, 0, reverse, 1, num, den,
                                    dmotor::timing::zero_timeout_max_cycles, speed, age, status);
                    dmotor_velocity(false, test_timing::tick_cycles, 1, test_timing::tick_cycles, 0,
                                    0, reverse, 1, num, den,
                                    dmotor::timing::zero_timeout_max_cycles, speed, age, status);
                    dmotor_velocity(false, (2 * test_timing::tick_cycles), 1 + events,
                                    (2 * test_timing::tick_cycles), 0, 0, reverse, 1, num, den,
                                    dmotor::timing::zero_timeout_max_cycles, speed, age, status);
                    const uint64_t numerator =
                        uint64_t{events} * (60ULL * dmotor::timing::fabric_hz * 1000) * den;
                    const uint64_t denominator = uint64_t{test_timing::tick_cycles} * num;
                    const uint64_t expected = (numerator + denominator / 2) / denominator;
                    if (expected > 1000000) {
                        CHECK((status >> 8) == 8 && speed == 0 && !(status & 1));
                    } else {
                        const int32_t signed_speed = static_cast<int32_t>(expected);
                        CHECK((status >> 8) == 0 && (status & 1) && age == 0);
                        CHECK(speed == (reverse ? -signed_speed : signed_speed));
                        CHECK(bool(status & 4) == (events > 1));
                    }
                }
            }
        }
    }
    for (bool bad_num : {false, true}) {
        const uint32_t num = bad_num ? 1048577U : 1U;
        const uint32_t den = bad_num ? 1U : 16385U;
        dmotor_velocity(true, 0, 0, 0, 0, 0, false, 1, num, den, test_timing::cycles_us(100000),
                        speed, age, status);
        dmotor_velocity(false, 0, 0, 0, 0, 0, false, 1, num, den, test_timing::cycles_us(100000),
                        speed, age, status);
        CHECK((status >> 8) == 1 && speed == 0 && !(status & 1));
    }
    std::cout << "velocity: both-direction 1/5/30/130 RPM sweep, zero timeout, restart, reversal, "
                 "invalid edges and snapshot faults passed\n";
}
