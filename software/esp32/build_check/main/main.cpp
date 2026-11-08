#include "monitor_inputs.hpp"
#include "monitor_task.hpp"
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

// Complete acquisition/policy target compile check. Not invoked by app_main;
// no default calibration, thresholds, motion source or motor authorization.
void check_monitor_cycle(dmotor_monitor::EspClock& clock,dmotor_monitor::EspBus& bus,
                         dmotor_monitor::EspMonitorInputs& inputs,
                         dmotor_monitor::MonitorSafety& safety,
                         dmotor_monitor::SensorConfig sensors,
                         dmotor_monitor::Calibration calibration,
                         dmotor_monitor::CurrentCalibration current,
                         dmotor_monitor::PolicyConfig policy) {
    inputs.start();
    dmotor_monitor::MonitorCycle cycle(clock,bus,inputs,sensors,calibration,current,1,policy);
    const auto release=clock.now(); auto token=safety.begin(release);
    const auto result=cycle.run(release);
    safety.finish(token,result.proof);
}

void check_monitor_task(dmotor_monitor::EspMonitorTask& task) {
    task.start(); (void)task.snapshot();
}
