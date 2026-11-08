#pragma once
#include "sensor_health.hpp"
namespace dmotor_monitor {
struct SafetyProof {
    unsigned population=0, valid_sensors=0;
    bool battery0=false, battery1=false, calibration=false, policy=false;
    bool recovery=false, physical_arm=false;
    bool monitoring_complete=false; // Current cycle evaluated; independent of policy approval.
    Us oldest_conversion=0;
};
struct WorkToken { uint64_t epoch=0, cycle=0; };
// All methods AND physical writes must share one adapter lock. No ISR or worker
// can write outputs independently. Faults latch until reboot; live recovery is
// deliberately absent. The current GPIO adapter is hard-inhibited; it has no enable switch.
class SafetyCore {
    const unsigned population_;
    uint64_t epoch_=0, cycle_=0;
    Us release_=-1, last_now_=0, hold_until_=0;
    bool active_=false, fault_=false, permit_=false, hb_=false, live_=false, locked_=false;
    bool time(Us now) {
        if(now<last_now_) { trip(); locked_=true; return false; }
        last_now_=now; return true;
    }
public:
    explicit SafetyCore(unsigned population=15) : population_(population) {}
    void trip() {
        if(epoch_!=UINT64_MAX) ++epoch_;
        else locked_=true;
        fault_=true; active_=false; permit_=false; hb_=false; live_=false;
    }
    WorkToken begin(Us release, Us now) {
        observe(now);
        if(locked_) return {epoch_,cycle_};
        if(release<0 || release>std::numeric_limits<Us>::max()-monitor_period_us-monitor_work_us ||
           now<release || now-release>=adc_budget_us || active_ || cycle_==UINT64_MAX) {
            trip(); return {epoch_,cycle_};
        }
        if(release_>=0 && release<=release_) { trip(); return {epoch_,cycle_}; }
        if(release_>=0 && release-release_!=monitor_period_us) trip();
        if(locked_) return {epoch_,cycle_};
        release_=release; ++cycle_; active_=true;
        return {epoch_,cycle_};
    }
    void observe(Us now) {
        if(!time(now)) return;
        if((active_ && now>=release_+monitor_work_us) ||
           (live_ && now>=hold_until_)) trip();
    }
    bool finish(WorkToken token, Us now, const SafetyProof& p) {
        observe(now);
        if(locked_ || !active_ || token.epoch!=epoch_ || token.cycle!=cycle_) return false;
        if(!population_ || population_>15 || p.population!=population_ || p.valid_sensors!=population_ ||
           !p.battery0 || !p.battery1 || !p.calibration || !p.monitoring_complete ||
           p.oldest_conversion<0 ||
           p.oldest_conversion>now || now-p.oldest_conversion>=25000) {
            trip(); return false;
        }
        active_=false;
        if(!p.policy && !fault_) {
            if(epoch_==UINT64_MAX) { trip(); return false; }
            ++epoch_; fault_=true;
        }
        permit_=!fault_ && p.policy && p.recovery && p.physical_arm;
        hb_=!hb_; live_=true;
        // D10: age qualifies a sample when committing a cycle, not a promise
        // of continuous motor-current protection. Liveness expires independently
        // if the next scheduled cycle fails to commit within its work budget.
        hold_until_=release_+monitor_period_us+monitor_work_us;
        return permit_; // Return value is motor permission, not heartbeat success.
    }
    bool permits() const { return permit_ && !fault_; }
    bool heartbeat() const { return hb_ && live_; }
    bool monitoring_live() const { return live_; }
    bool faulted() const { return fault_; }
    uint64_t epoch() const { return epoch_; }
};
} // namespace dmotor_monitor
