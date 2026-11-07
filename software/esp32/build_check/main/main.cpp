#include "monitor_i2c.hpp"
// Link check only: no GPIO, acquisition task, bus initialization or motor authority.
extern "C" void app_main() {
    dmotor_monitor::EspClock clock;
    dmotor_monitor::EspBus bus({});
    dmotor_monitor::Acquisition acquisition(clock, bus);
    (void)acquisition.faulted();
}
