#pragma once
#include "sensor_health.hpp"
namespace dmotor_monitor {
// Local implementation causes, not UART cause-bit assignments.
enum PolicyCause : uint32_t { config_fault=1, input_fault=2, hard_fault=4,
    current_fault=8, supply_fault=16, stall_fault=32, battery_fault=64, timing_fault=128 };
struct Threshold {
    double trip=0, release=0;
    Us persistence=0;
    bool valid() const {
        return std::isfinite(trip) && std::isfinite(release) && trip>release &&
               release>=0 && persistence>=0 && persistence<=60000000;
    }
};
struct MotorPolicy {
    Threshold winding_ma, supply_ma;
    double stall_ma=0;
    Us stall_us=0;
    bool use_motion=false;
    Us motion_max_age_us=0;
    bool valid() const {
        return winding_ma.valid() && supply_ma.valid() && std::isfinite(stall_ma) &&
            stall_ma>0 && stall_us>0 && stall_us<=60000000 &&
            (!use_motion || (motion_max_age_us>0 && motion_max_age_us<=1000000));
    }
};
struct PolicyConfig {
    unsigned population=0;
    // No production thresholds or calibration inferred from motor-damage waiver.
    std::array<MotorPolicy,4> motor{};
    double pack_min_mv=0, pack_max_mv=0;
    double pair_low_mv=0, pair_recover_mv=0;
    Us low_persistence_us=0;
    Threshold imbalance_mv;
    bool valid() const {
        if(!population || population>15 || !std::isfinite(pack_min_mv) ||
           !std::isfinite(pack_max_mv) || pack_min_mv<=0 || pack_max_mv<=pack_min_mv ||
           !std::isfinite(pair_low_mv) || !std::isfinite(pair_recover_mv) ||
           pair_low_mv<=0 || pair_recover_mv<=pair_low_mv ||
           low_persistence_us<0 || low_persistence_us>60000000 || !imbalance_mv.valid()) return false;
        for(unsigned i=0;i<4;++i) if((population&(1u<<i)) && !motor[i].valid()) return false;
        return true;
    }
};
struct MotionObservation {
    bool valid=false, commanded=false, moving=false;
    Us observed=0;
};
struct PolicyInput {
    unsigned population=0, valid_current=0;
    bool batteries_valid=false;
    uint32_t hard_causes=0; // Normalized STOP/HALT/nFAULT/ALERT, asserted => inhibit.
    double full_mv=0, midpoint_mv=0;
    std::array<double,4> winding_ma{}, supply_ma{};
    std::array<MotionObservation,4> motion{};
};
struct PolicyResult { bool healthy=false; uint32_t causes=0; unsigned motors=0; bool monitoring_complete=false; };
// Persistence measured in elapsed microseconds; hysteresis holds an excursion
// until it falls below the release level. Latched faults cannot clear by a good
// sample. Startup/config reload/recovery must be explicitly handled inhibited.
class MonitorPolicy {
    struct Timer {
        Us since=-1;
        bool update(bool trip,bool clear,Us duration,Us now) {
            if(clear) since=-1;
            else if(trip && since<0) since=now;
            return since>=0 && now-since>=duration;
        }
    };
    PolicyConfig cfg_;
    std::array<Timer,4> current_{}, supply_{}, stall_{};
    Timer low_{}, imbalance_{};
    Us previous_=-1;
    uint32_t latched_=0;
    unsigned motors_=0;
public:
    explicit MonitorPolicy(PolicyConfig cfg):cfg_(cfg) {}
    void invalidate(uint32_t cause) { latched_|=cause; }
    PolicyResult evaluate(Us release,Us now,const PolicyInput& in) {
        bool complete=false; uint32_t invalid=0;
        auto result=[&]() { return PolicyResult{latched_==0,latched_,motors_,complete}; };
        if(!cfg_.valid() || in.population!=cfg_.population) invalid|=config_fault;
        if(release<0 || now<release || now-release>=monitor_work_us ||
           (previous_>=0 && (release<=previous_ || release-previous_!=monitor_period_us)))
            invalid|=timing_fault;
        previous_=release;
        if(in.hard_causes) latched_|=hard_fault;
        if(!in.batteries_valid || in.valid_current!=cfg_.population ||
           !std::isfinite(in.full_mv) || !std::isfinite(in.midpoint_mv) ||
           in.midpoint_mv<=0 || in.midpoint_mv>=in.full_mv) invalid|=input_fault;
        for(unsigned i=0;i<4;++i) if(cfg_.population&(1u<<i)) {
            if(!std::isfinite(in.winding_ma[i]) || !std::isfinite(in.supply_ma[i]) ||
               in.winding_ma[i]<0) invalid|=input_fault;
            const auto& m=in.motion[i]; const auto& c=cfg_.motor[i];
            if(c.use_motion && (!m.valid || m.observed<0 || m.observed>now ||
               now-m.observed>=c.motion_max_age_us)) invalid|=input_fault;
        }
        latched_|=invalid;
        if(invalid) return result();
        complete=true;
        if(in.full_mv<cfg_.pack_min_mv || in.full_mv>cfg_.pack_max_mv) latched_|=battery_fault;
        const double lower=in.midpoint_mv, upper=in.full_mv-in.midpoint_mv;
        if(low_.update(lower<cfg_.pair_low_mv || upper<cfg_.pair_low_mv,
                       lower>=cfg_.pair_recover_mv && upper>=cfg_.pair_recover_mv,
                       cfg_.low_persistence_us,now)) latched_|=battery_fault;
        const double delta=std::abs(upper-lower);
        if(imbalance_.update(delta>=cfg_.imbalance_mv.trip,delta<=cfg_.imbalance_mv.release,
                            cfg_.imbalance_mv.persistence,now)) latched_|=battery_fault;
        for(unsigned i=0;i<4;++i) if(cfg_.population&(1u<<i)) {
            const auto& c=cfg_.motor[i];
            const double w=in.winding_ma[i], f=std::abs(in.supply_ma[i]);
            uint32_t cause=0;
            if(current_[i].update(w>=c.winding_ma.trip,w<=c.winding_ma.release,
                                 c.winding_ma.persistence,now)) cause|=current_fault;
            if(supply_[i].update(f>=c.supply_ma.trip,f<=c.supply_ma.release,
                                c.supply_ma.persistence,now)) cause|=supply_fault;
            const bool suspect=w>=c.stall_ma && (!c.use_motion ||
                (in.motion[i].commanded && !in.motion[i].moving));
            if(stall_[i].update(suspect,!suspect,c.stall_us,now)) cause|=stall_fault;
            // Without motion evidence this is a sustained-current stall proxy,
            // not proof of a mechanically stalled rotor.
            if(cause) { latched_|=cause; motors_|=1u<<i; }
        }
        return result();
    }
};
} // namespace dmotor_monitor
