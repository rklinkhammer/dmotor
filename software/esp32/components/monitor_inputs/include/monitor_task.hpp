#pragma once
#include "monitor_inputs.hpp"
#include "monitor_safety.hpp"
#include "release_wait.hpp"
#include "esp_rom_sys.h"
namespace dmotor_monitor {
// Injectable motion evidence only. No new UART contract or guessed encoder data.
struct MotionSource {
    virtual std::array<MotionObservation,4> snapshot()=0;
    virtual ~MotionSource()=default;
};
// Application-owned permanent object; one worker owns ADC/I2C/calibration/policy.
// Bus devices must already be configured inhibited. All physical outputs remain
// LOW through MonitorSafety; this does not implement live recovery/ARM authority.
class EspMonitorTask {
    Clock& clock_; EspMonitorInputs& inputs_; MotionSource* motion_;
    MonitorCycle cycle_; MonitorSafety safety_;
    bool configured_, started_=false;
    portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
    MonitorCycleResult latest_{};
    static void entry(void* arg) { static_cast<EspMonitorTask*>(arg)->run(); }
    void run() {
        Us release=clock_.now(); bool initial=true;
        for(;;) {
            if(!wait_for_release(clock_,release,[](Us remaining) {
                if(remaining>=1000) vTaskDelay(1);
                else esp_rom_delay_us(static_cast<uint32_t>(remaining));
            })) { safety_.fault(); vTaskDelete(nullptr); return; }
            WorkToken token{};
            // Initial ready-flag baseline has no authority. All later work is
            // observed against its independent deadline, even while disarmed.
            if(!initial) token=safety_.begin(release);
            const auto motion=motion_?motion_->snapshot():std::array<MotionObservation,4>{};
            const auto result=cycle_.run(release,motion,false,false);
            if(initial) {
                if(!result.warming) safety_.fault();
                initial=false;
            } else safety_.finish(token,result.proof);
            portENTER_CRITICAL(&lock_); latest_=result; portEXIT_CRITICAL(&lock_);
            if(release>std::numeric_limits<Us>::max()-monitor_period_us-monitor_work_us) {
                safety_.fault(); vTaskDelete(nullptr); return;
            }
            release+=monitor_period_us;
        }
    }
public:
    EspMonitorTask(Clock& clock,Bus& bus,EspMonitorInputs& inputs,SensorConfig sensors,
                   Calibration calibration,CurrentCalibration current,uint32_t board,
                   PolicyConfig policy,MotionSource* motion=nullptr)
        :clock_(clock),inputs_(inputs),motion_(motion),
         cycle_(clock,bus,inputs,sensors,calibration,current,board,policy),
         safety_(sensors.population),
         configured_(sensors.valid() && calibration.valid(board,sensors.population) &&
             current.valid(board,sensors.population) && policy.valid() &&
             policy.population==sensors.population) {
        for(unsigned i=0;i<4;++i) if((sensors.population&(1u<<i)) &&
            policy.motor[i].use_motion && !motion) configured_=false;
    }
    bool start() {
        if(started_) return false;
        started_=true;
        if(!safety_.start()) return false;
        if(!configured_ || !inputs_.start()) { safety_.fault(); return false; }
        if(xTaskCreate(entry,"monitor_work",6144,this,5,nullptr)!=pdPASS) {
            safety_.fault(); return false;
        }
        return true;
    }
    MonitorCycleResult snapshot() {
        portENTER_CRITICAL(&lock_); auto r=latest_; portEXIT_CRITICAL(&lock_); return r;
    }
};
} // namespace dmotor_monitor
