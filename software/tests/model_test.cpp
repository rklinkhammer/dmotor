#include "dmotor_sw/model.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#define CHECK(expr) do { if (!(expr)) { std::cerr << __LINE__ << ": " #expr "\n"; std::exit(1); } } while (false)
using namespace dmotor_sw;
std::array<MotorLimits, motor_count> limits() {
    std::array<MotorLimits, motor_count> result{};
    for (auto& motor : result) motor = {1, 200000};
    return result;
}
dmotor::VelocityBatch batch(uint32_t sequence, uint32_t mask = all_motors) {
    dmotor::VelocityBatch result;
    result.session = 1; result.sequence = sequence; result.mask = mask;
    for (unsigned m = 0; m < motor_count; ++m) {
        result.generation[m] = 1;
        result.velocity_mrpm[m] = m % 2 ? -130000 : 130000;
    }
    return result;
}
int main() {
    {
        Model model({10, 5, 100});
        CHECK(model.snapshot().halted);
        MaintenanceSession owner(model);
        CHECK(owner.token()); CHECK(!model.acquire());
        CHECK(!model.rearm(owner.token() + 1, 1, limits()));
        CHECK(model.rearm(owner.token(), 1, limits()));
        CHECK(model.enable(owner.token(), all_motors));
        CHECK(model.publish(owner.token(), batch(1, 3)));
        CHECK(!model.publish(owner.token(), batch(2, 1))); // Partial overlap.
        CHECK(!model.publish(owner.token(), batch(2, 12))); // Disjoint still different.
        CHECK(model.publish(owner.token(), batch(2, 3))); // Identical-mask replacement.
        CHECK(!model.snapshot().tracking_mask);
        model.advance();
        auto state = model.snapshot();
        CHECK(state.applied.sequence == 2 && state.applied.mask == 3 && state.applied.tick == 1);
        CHECK(state.requested_mrpm[0] == 130000 && state.requested_mrpm[1] == -130000);
        CHECK(!state.requested_mrpm[2]);
        CHECK(!model.publish(owner.token(), batch(2, 3))); // Replay.
        auto invalid = batch(3, 3); invalid.generation[1] = 2;
        CHECK(!model.publish(owner.token(), invalid));
        invalid = batch(3, 3); invalid.velocity_mrpm[0] = std::numeric_limits<int32_t>::min();
        CHECK(!model.publish(owner.token(), invalid));
        model.advance(4);
        CHECK(model.publish(owner.token(), batch(3, 3))); // Too late: pending does not renew age.
        CHECK(model.refresh(owner.token(), all_motors));
        model.advance();
        CHECK(model.snapshot().halted && model.snapshot().fault == Fault::target_age);
        CHECK(!model.snapshot().enabled_mask);
        CHECK(model.snapshot().applied.sequence == 2);
        CHECK(!model.rearm(owner.token(), 1, limits()));
        CHECK(model.rearm(owner.token(), 2, limits()));
        CHECK(!model.snapshot().enabled_mask && !model.snapshot().tracking_mask);
        CHECK(!model.publish(owner.token(), batch(4)));
    }
    {
        Model model({3, 100, 100});
        const auto owner = model.acquire();
        CHECK(model.rearm(owner, 1, limits())); CHECK(model.enable(owner, 1));
        model.advance(2); CHECK(model.refresh(owner, 1)); model.advance();
        CHECK(model.snapshot().fault == Fault::authorization); // READY also expires; boundary wins.
    }
    {
        Model model({5, 3, 100}); const auto owner = model.acquire();
        CHECK(model.rearm(owner, 1, limits())); CHECK(model.enable(owner, 1));
        for (uint32_t sequence = 1; sequence <= 10; ++sequence) {
            auto target = batch(sequence, 1); target.velocity_mrpm[0] = 0;
            CHECK(model.publish(owner, target)); CHECK(model.refresh(owner, 1)); model.advance();
            CHECK(!model.snapshot().halted);
        }
        CHECK(model.snapshot().tracking_mask == 1); // Zero target still tracking, not STOP.
        for (unsigned i = 0; i < 3; ++i) { CHECK(model.refresh(owner, 1)); model.advance(); }
        CHECK(model.snapshot().fault == Fault::target_age);
    }
    {
        Model model({100, 100, 4});
        uint64_t old_owner = 0;
        {
            MaintenanceSession owner(model); old_owner = owner.token();
            CHECK(model.rearm(old_owner, 1, limits())); CHECK(model.enable(old_owner, all_motors));
            CHECK(model.publish(old_owner, batch(1))); model.advance();
        }
        CHECK(model.snapshot().halted && !model.snapshot().enabled_mask);
        const auto next = model.acquire(); CHECK(next && next != old_owner);
        CHECK(!model.renew(old_owner)); CHECK(!model.release(old_owner));
        CHECK(model.snapshot().halted); // Reopening never rearms.
        CHECK(model.rearm(next, 2, limits())); CHECK(model.enable(next, all_motors));
        model.advance(3); CHECK(model.renew(next)); model.advance(3);
        CHECK(!model.snapshot().halted); model.advance(); // Simulated crash: no destructor.
        CHECK(model.snapshot().halted && model.snapshot().fault == Fault::ownership_lost);
        CHECK(!model.renew(next)); CHECK(model.acquire());
    }
    bool rejected = false;
    try { Model invalid({0, 1, 1}); } catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
    std::cout << "PASS: mailbox atomicity, replay/range checks, timeout precedence, ownership and HALT lifecycle\n";
}
