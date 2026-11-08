#include "monitor_safety.hpp"
#include "physical_acquisition.hpp"
#include "monitor_i2c.hpp"
// Link check only: no GPIO, acquisition task, bus initialization or motor authority.
extern "C" void app_main() {
    dmotor_monitor::EspClock clock;
    dmotor_monitor::EspBus bus({});
    dmotor_monitor::Acquisition acquisition(clock, bus);
    (void)acquisition.faulted();
}

// Compile/link the full adapter without starting tasks or touching GPIO.
void check_safety_adapter(dmotor_monitor::MonitorSafety& safety,
                          dmotor_monitor::PhysicalAcquisition& acquisition) {
    auto result=acquisition.run(0);
    (void)result;
    safety.start();
    auto token=safety.begin(0);
    safety.finish(token,{});
    safety.fault();
}
