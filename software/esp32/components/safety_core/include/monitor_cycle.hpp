#pragma once
#include "monitor_policy.hpp"
#include "physical_acquisition.hpp"
#include "safety_core.hpp"
namespace dmotor_monitor {
struct MonitorInputs {
    // ADC indices: full battery, midpoint, IPROPI0..3. GPIO1/2/4..7 on ESP32-S3.
    virtual bool adc(unsigned channel,int& raw)=0;
    virtual uint32_t faults()=0;
    virtual ~MonitorInputs()=default;
};
struct CurrentCalibration {
    uint32_t version=1, generation=0, board=0, population=0;
    std::array<int32_t,4> gain_ppm{}, offset_milli{};
    uint32_t checksum=0;
    uint32_t crc() const {
        uint32_t c=0xffffffffu;
        auto word=[&](uint32_t w) {
            for(unsigned i=0;i<4;++i) {
                c^=(w>>(8*i))&255u;
                for(unsigned j=0;j<8;++j) c=(c>>1)^((c&1)?0xedb88320u:0u);
            }
        };
        word(version); word(generation); word(board); word(population);
        for(auto v:gain_ppm) word(static_cast<uint32_t>(v));
        for(auto v:offset_milli) word(static_cast<uint32_t>(v));
        return ~c;
    }
    bool valid(uint32_t id,unsigned mask) const {
        if(version!=1 || !generation || !id || board!=id || !mask || mask>15 ||
           population!=mask || checksum!=crc()) return false;
        for(unsigned i=0;i<4;++i) if((mask&(1u<<i)) && gain_ppm[i]<=0) return false;
        return true;
    }
};
struct MonitorCycleResult {
    SafetyProof proof{};
    PolicyResult policy{};
    PhysicalResult sensors{};
    std::array<int,6> adc{};
    unsigned adc_reads=0;
    Us adc_end=0, finished=0;
    bool warming=false;
};
// Single-owner complete workload: GPIO + both battery ADCs + installed IPROPI
// ADCs + all six INA registers/device + calibration + policy. No invented sensor
// population or silently relaxed deadline. Independent supervisor stays separate.
class MonitorCycle {
    Clock& clock_; MonitorInputs& inputs_; PhysicalAcquisition acquisition_;
    SensorConfig sensors_; Calibration calibration_; CurrentCalibration current_;
    uint32_t board_; MonitorPolicy policy_; bool initialized_=false;
    bool config_ok_;
public:
    MonitorCycle(Clock& clock,Bus& bus,MonitorInputs& inputs,SensorConfig sensors,
                 Calibration calibration,CurrentCalibration current,uint32_t board,
                 PolicyConfig policy)
        :clock_(clock),inputs_(inputs),acquisition_(clock,bus,sensors),sensors_(sensors),
         calibration_(calibration),current_(current),board_(board),policy_(policy),
         config_ok_(sensors.valid() && policy.valid() && policy.population==sensors.population) {}
    MonitorCycleResult run(Us release,const std::array<MotionObservation,4>& motion={},
                           bool recovered=false,bool armed=false) {
        MonitorCycleResult out;
        PolicyInput in; in.population=sensors_.population; in.motion=motion;
        in.hard_causes=inputs_.faults();
        auto fail=[&](uint32_t cause) {
            out.finished=clock_.now();
            policy_.invalidate(cause);
            out.policy=policy_.evaluate(release,out.finished,in);
            return out;
        };
        if(!config_ok_ || release<0 ||
           release>std::numeric_limits<Us>::max()-monitor_period_us) return fail(config_fault);
        Us previous=clock_.now();
        if(previous<release || previous-release>=adc_budget_us) return fail(timing_fault);
        for(unsigned channel=0;channel<6;++channel) {
            if(channel>=2 && !(sensors_.population&(1u<<(channel-2)))) continue;
            const Us before=clock_.now();
            if(before<previous || before-release>=adc_budget_us) return fail(timing_fault);
            int raw=0; const bool ok=inputs_.adc(channel,raw); const Us after=clock_.now();
            if(after<before || after-release>=adc_budget_us) return fail(timing_fault);
            // Explicit rail rejection; calibration cannot turn a clipped ADC into health.
            if(!ok || raw<=0 || raw>=4095) return fail(input_fault);
            out.adc[channel]=raw; ++out.adc_reads; previous=after;
        }
        out.adc_end=clock_.now();
        const Us offset=out.adc_end-release+ina_budget_us;
        out.sensors=acquisition_.run(release,offset<acquisition_deadline_us?offset:acquisition_deadline_us);
        const Us now=clock_.now();
        if(now<out.adc_end || now-release>=acquisition_deadline_us) return fail(timing_fault);
        in.hard_causes|=inputs_.faults();
        out.proof.population=sensors_.population;
        out.proof.valid_sensors=out.sensors.valid;
        out.proof.calibration=calibration_.valid(board_,sensors_.population) &&
                              current_.valid(board_,sensors_.population);
        if(!out.proof.calibration) return fail(config_fault);
        if(!calibration_.convert(8,out.adc[0],board_,sensors_.population,in.full_mv) ||
           !calibration_.convert(9,out.adc[1],board_,sensors_.population,in.midpoint_mv))
            return fail(config_fault);
        in.batteries_valid=true;
        bool all_good=true, only_warm_or_good=true;
        out.proof.oldest_conversion=now;
        for(unsigned d=0;d<4;++d) if(sensors_.population&(1u<<d)) {
            const auto& m=out.sensors.device[d];
            all_good=all_good && m.health==Health::good;
            only_warm_or_good=only_warm_or_good && (m.health==Health::warming || m.health==Health::good);
            if(m.health!=Health::good || m.oldest_possible_us>now || now-m.oldest_possible_us>=25000)
                continue;
            if(!calibration_.convert(d,m.shunt_raw,board_,sensors_.population,in.supply_ma[d]))
                return fail(config_fault);
            in.winding_ma[d]=double(out.adc[d+2])*current_.gain_ppm[d]/1000000.0+
                              current_.offset_milli[d]/1000.0;
            in.valid_current|=1u<<d;
            if(m.oldest_possible_us<out.proof.oldest_conversion)
                out.proof.oldest_conversion=m.oldest_possible_us;
        }
        // Exactly one initial baseline is allowed while inhibited. Warming after
        // missing data never clears a policy latch. Supervisor authorization is
        // deliberately absent on this return.
        const bool initial=!initialized_; initialized_=true;
        if(initial && !all_good && only_warm_or_good && out.sensors.complete && !in.hard_causes) {
            out.warming=true; out.finished=clock_.now(); return out;
        }
        out.policy=policy_.evaluate(release,clock_.now(),in);
        out.finished=clock_.now();
        if(out.finished<now || out.finished-release>=monitor_work_us) return fail(timing_fault);
        out.proof.battery0=out.proof.battery1=in.batteries_valid;
        out.proof.policy=out.policy.healthy;
        out.proof.monitoring_complete=out.policy.monitoring_complete;
        out.proof.recovery=recovered; out.proof.physical_arm=armed;
        return out;
    }
};
} // namespace dmotor_monitor
