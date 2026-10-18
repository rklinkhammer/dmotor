#include "dmotor_sw/runtime.hpp"
#include "dmotor/controller_registers.hpp"
#include "dmotor/timing.hpp"
#include <limits>
namespace dmotor_sw {
namespace reg = dmotor::controller_reg;
namespace batch = dmotor::batch_reg;
namespace {
constexpr uint32_t all = 15;
constexpr uint32_t poll_us = 100;
constexpr uint32_t clocks_per_us = dmotor::timing::clocks_per_us;
struct Adapter {
    Platform& io;
    uint32_t read(uint32_t a) { return io.read32(a); }
    bool write(uint32_t a,uint32_t v) { io.write32(a,v); return true; }
    void fence() { io.fence(); }
};
void require(bool good, const char* message) { if (!good) throw IoError(message); }
}
Runtime::Runtime(Platform& io, RuntimePolicy policy) : io_(io), protocol_(io), policy_(policy) {
    require(policy.authorization_us >= 1000 && policy.authorization_us <= 1000000 &&
            policy.refresh_period_us && uint64_t{policy.refresh_period_us}*3 <= policy.authorization_us &&
            policy.target_period_us && uint64_t{policy.target_period_us}*3 <= policy.target_timeout_us &&
            policy.target_timeout_us >= 1000 && policy.target_timeout_us <= 1000000 &&
            policy.client_lease_us && policy.readiness_us && policy.transition_us, "Invalid runtime timing policy");
    require(!policy.direct_mask || (policy.commissioning && policy.direct_mask<=8 &&
        !(policy.direct_mask&(policy.direct_mask-1)) && policy.duty_limit_q16<=65536 &&
        policy.duty_floor_q16<=policy.duty_limit_q16),"Invalid direct-duty policy");
    require(!policy.direct_pulse_us || (policy.direct_mask && policy.direct_pulse_us<=powered_pulse_limit_us),
            "Bounded pulse requires direct duty and at most one second");
    halt();
}
Runtime::~Runtime() { try { halt(); } catch (...) {} }
void Runtime::write(uint32_t a,uint32_t v) { io_.write32(a,v); io_.fence(); }
void Runtime::wait_status(uint32_t mask,uint32_t expected) {
    const auto start=io_.now_us();
    while ((io_.read32(reg::STATUS)&mask)!=expected) {
        require(io_.now_us()-start < policy_.transition_us,"Lifecycle transition timeout");
        io_.sleep_us(poll_us);
    }
}
void Runtime::halt() {
    pulse_active_=false;
    injection_=TimeoutInjection::none;
    halted_=true; prepared_=false; enabled_=tracking_=stopped_=0; targets_.fill(0);
    io_.halt(true); // Platform must confirm inhibition or throw; never claim success on access loss.
}
void Runtime::inject_timeout(TimeoutInjection injection) {
    require(policy_.commissioning && !policy_.direct_pulse_us,"Timeout injection requires inhibited commissioning");
    require(prepared_ && !halted_ && enabled_ && tracking_ && applied_ &&
            injection_==TimeoutInjection::none && injection!=TimeoutInjection::none,
            "Injection requires active applied velocity; rearm between injections");
    injection_=injection;
    // Independent host fallback if the FPGA does not trip. Never auto-resume.
    const auto budget=injection==TimeoutInjection::target ? policy_.target_timeout_us : policy_.authorization_us;
    injection_deadline_=io_.now_us()+budget+policy_.transition_us;
}
void Runtime::heartbeat() { lease_=io_.now_us(); }
ControllerStatus Runtime::status() { return protocol_.status(); }
TelemetrySnapshot Runtime::capture() {
    TelemetrySnapshot result; require(protocol_.capture(result),"Incoherent telemetry"); return result;
}
void Runtime::prepare(const std::array<MotorConfiguration,4>& configs,uint32_t mask) {
    require(mask && !(mask&~all) && (!policy_.direct_mask || mask==policy_.direct_mask),"Invalid configuration mask");
    for(unsigned m=0;m<4;++m) if(mask&(1U<<m)) require(valid_configuration(configs[m]),"Invalid motor configuration");
    try {
        halt(); wait_status(all,0);
        require(protocol_.compatible_controller(),"Unsupported controller");
        const auto old=io_.read32(reg::SESSION);
        require(old!=UINT32_MAX,"Session exhausted; isolated fabric reset required");
        if(old) write(reg::CORE_RESET,1);
        session_=old+1;
        write(reg::SESSION,session_);
        require(io_.read32(reg::SESSION)==session_,"Session not accepted");
        write(reg::CORE_RESET,1);
        for(unsigned m=0;m<4;++m) if(mask&(1U<<m)) {
            require(protocol_.configure(m,configs[m]),"Configuration rejected");
            const auto words=encode_configuration(configs[m]);
            for(unsigned w=0;w<configuration_words;++w)
                require(io_.read32(reg::CONFIG_BASE+m*reg::CONFIG_STRIDE+w*4)==words[w],"Configuration readback mismatch");
        }
        if(policy_.direct_mask) {
            require(io_.read32(reg::DIRECT_CAPABILITY)==0x44445531U,"Bitstream lacks direct-duty capability");
            write(reg::DIRECT_MODE,policy_.direct_mask);
            write(reg::DIRECT_LIMIT,policy_.duty_limit_q16);
            require(io_.read32(reg::DIRECT_MODE)==policy_.direct_mask && io_.read32(reg::DIRECT_LIMIT)==policy_.duty_limit_q16,"Direct-duty configuration rejected");
        } else {
            write(reg::DIRECT_MODE,0);
            require(io_.read32(reg::DIRECT_MODE)==0,"Direct-duty mode did not clear");
        }
        write(reg::WATCHDOG_CYCLES,policy_.authorization_us*clocks_per_us);
        write(reg::TARGET_TIMEOUT_CYCLES,policy_.target_timeout_us*clocks_per_us);
        io_.halt(false); io_.sleep_us(poll_us);
        write(reg::ACKNOWLEDGE,1); io_.sleep_us(poll_us);
        write(reg::ACKNOWLEDGE,2); io_.sleep_us(poll_us);
        write(reg::ACKNOWLEDGE,4); io_.sleep_us(poll_us);
        write(reg::CORE_RESET,0);
        wait_status((1U<<16)|(1U<<17)|(1U<<18),0);
        require(policy_.direct_mask || protocol_.compatible_mailbox(),"Unsupported mailbox");
        configs_=configs;configured_=mask;sequence_=applied_=0;refresh_sequence_.fill(0);
        halted_=false;prepared_=true;heartbeat();
        next_refresh_=next_target_=io_.now_us();
    } catch (...) { try { halt(); } catch (...) {} throw; }
}
void Runtime::enable(uint32_t mask) {
    require(prepared_ && !halted_ && mask && !(mask&~configured_) && !(mask&stopped_),"Prepare/rearm required before enable");
    const auto added=mask&~enabled_;
    for(unsigned m=0;m<4;++m) if(added&(1U<<m)) refresh_sequence_[m]=0;
    write(reg::ENABLE,mask); enabled_|=mask;
    ready_deadline_=io_.now_us()+policy_.readiness_us;
    next_refresh_=io_.now_us();
}
void Runtime::velocity(uint32_t mask,const std::array<int32_t,4>& targets) {
    require(!policy_.direct_mask,"Velocity command unavailable in direct-duty mode");
    require(prepared_ && !halted_ && mask && !(mask&~enabled_) && !(mask&stopped_),"Motor unavailable");
    require((status().ready&mask)==mask,"Motors not ready");
    for(unsigned m=0;m<4;++m) if(mask&(1U<<m)) {
        const int64_t v=targets[m]; require(v<=configs_[m].maximum_mrpm && v>=-int64_t{configs_[m].maximum_mrpm},"Per-motor speed limit");
    }
    // Publish the union of all tracking motors as one group. A changed mask waits
    // for the existing pending group to retire; never partially replace it.
    for(unsigned m=0;m<4;++m) if(mask&(1U<<m)) targets_[m]=targets[m];
    tracking_|=mask; next_target_=io_.now_us();
}
void Runtime::duty(int32_t value) {
    require(policy_.direct_mask && prepared_ && !halted_ &&
        (enabled_&policy_.direct_mask)==policy_.direct_mask && !stopped_,"Direct-duty motor unavailable");
    const int64_t magnitude=value<0 ? -int64_t{value} : value;
    require(magnitude<=policy_.duty_limit_q16 && (!magnitude || magnitude>=policy_.duty_floor_q16),"Duty outside configured floor/limit (zero always allowed)");
    require((status().ready&policy_.direct_mask)==policy_.direct_mask,"Motor not ready");
    if(policy_.direct_pulse_us) {
        if(pulse_active_ && io_.now_us()-pulse_started_>=policy_.direct_pulse_us) {
            stop(policy_.direct_mask);
            throw IoError("Duty pulse expired; rearm required");
        }
        if(!value) { stop(policy_.direct_mask);pulse_active_=false;return; }
        if(!pulse_active_) { pulse_started_=io_.now_us();pulse_active_=true; }
    }
    for(unsigned m=0;m<4;++m)targets_[m]=value;
    tracking_=policy_.direct_mask;next_target_=io_.now_us();
}
void Runtime::stop(uint32_t mask) {
    require(mask && !(mask&~enabled_),"Invalid STOP mask");
    if(policy_.direct_mask) pulse_active_=false;
    write(reg::STOP,mask); if(policy_.direct_mask)enabled_&=~mask; tracking_&=~mask;stopped_|=mask;
    for(unsigned m=0;m<4;++m) if(mask&(1U<<m)) stop_deadline_[m]=io_.now_us()+configs_[m].stop_timeout_us;
}
void Runtime::disable(uint32_t mask) {
    require(mask && !(mask&~all),"Invalid disable mask");
    if(policy_.direct_mask) pulse_active_=false;
    write(reg::DISABLE,mask);enabled_&=~mask;tracking_&=~mask;
    // Re-enable always goes through global prepare to discard pending intent.
    stopped_|=mask;
}
void Runtime::refresh(uint32_t mask) {
    write(reg::REFRESH_SESSION,session_);
    for(unsigned m=0;m<4;++m) if(mask&(1U<<m)) {
        require(refresh_sequence_[m]!=UINT32_MAX,"Refresh sequence exhausted");
        write(reg::REFRESH_BASE+m*4,++refresh_sequence_[m]);
    }
}
void Runtime::tick() {
    if(halted_) return;
    try {
        const auto now=io_.now_us();
        // Check before publishing or refreshing. STOP disables direct drive and
        // makes a new pulse require explicit global rearm.
        if(pulse_active_ && now-pulse_started_>=policy_.direct_pulse_us) {
            stop(policy_.direct_mask);pulse_active_=false;
        }
        require(now-lease_<policy_.client_lease_us,"Maintenance lease expired");
        require(injection_==TimeoutInjection::none || now<injection_deadline_,"Injection deadline: FPGA trip not observed; host HALT");
        auto s=status();
        require(!s.fault && !s.shared_halt && !s.blocked,"Hardware protection active");
        require((s.enabled&enabled_)==enabled_,"Motor unexpectedly disabled");
        require((s.ready&(enabled_&~stopped_))==(enabled_&~stopped_) || now<ready_deadline_,"Readiness timeout");
        for(unsigned m=0;m<4;++m) if((stopped_&enabled_&(1U<<m)) && !(s.stopped&(1U<<m)))
            require(now<stop_deadline_[m],"Controlled STOP timeout");
        if(injection_!=TimeoutInjection::watchdog && enabled_ && now>=next_refresh_) { refresh(enabled_);next_refresh_=now+policy_.refresh_period_us; }
        if(injection_!=TimeoutInjection::target && tracking_ && now>=next_target_) {
            if(policy_.direct_mask) {
                require(sequence_!=UINT32_MAX,"Duty sequence exhausted");
                write(reg::DIRECT_DUTY,static_cast<uint32_t>(targets_[0]));
                write(reg::DIRECT_COMMIT,++sequence_);
                applied_=io_.read32(reg::DIRECT_COMMIT);
                require(applied_==sequence_,"Direct-duty command rejected");
                next_target_=now+policy_.target_period_us;
            } else {
            auto pending=io_.read32(batch::PENDING_MASK);
            if(!pending || pending==tracking_) {
                require(sequence_!=UINT32_MAX,"Target sequence exhausted");
                dmotor::VelocityBatch b{};b.session=session_;b.sequence=++sequence_;b.mask=tracking_;
                for(unsigned m=0;m<4;++m) { b.generation[m]=configs_[m].generation;b.velocity_mrpm[m]=targets_[m]; }
                Adapter adapter{io_}; require(dmotor::publish_velocity_batch(adapter,b),"Target publication rejected");
                next_target_=now+policy_.target_period_us;
            }
        }
        }
        if(enabled_ && !policy_.direct_mask) {
            Adapter adapter{io_};dmotor::AppliedBatchSnapshot sample;
            if(dmotor::read_applied_batch(adapter,sample)) applied_=sample.sequence;
        }
    } catch(...) { try { halt(); } catch(...) {} throw; }
}
}
