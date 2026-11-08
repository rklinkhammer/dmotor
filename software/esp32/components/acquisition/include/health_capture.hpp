#pragma once
#include "sensor_health.hpp"
namespace dmotor_monitor {
struct HealthRecord {
    uint32_t cycle=0;
    Health health=Health::warming;
    Us release=0, lateness=0, duration=0, age=-1;
    int32_t shunt=0, bus_error=0;
    uint16_t bus=0;
    unsigned attempts=0, error_reg=0;
    bool complete=false;
};
// Fixed RAM regardless of configured duration. No formatting in capture loop.
template<unsigned Capacity> struct HealthCapture {
    static_assert(Capacity>0,"capture must retain a tail");
    std::array<HealthRecord,Capacity> tail{};
    std::array<uint32_t,9> health_counts{};
    HealthRecord first_bad{};
    bool has_bad=false;
    uint32_t cycles=0, bus_errors=0, guarded=0, late_releases=0, deadline_misses=0;
    Us max_duration=0, max_lateness=0, max_age=-1;
    void add(const HealthRecord& r) {
        tail[cycles%Capacity]=r; ++cycles;
        const auto h=static_cast<unsigned>(r.health);
        if(h<health_counts.size()) ++health_counts[h];
        if(r.bus_error) ++bus_errors;
        if(!r.complete && !r.bus_error) ++guarded;
        if(r.lateness>=adc_budget_us) ++late_releases;
        if(r.duration>=acquisition_deadline_us) ++deadline_misses;
        if(r.duration>max_duration) max_duration=r.duration;
        if(r.lateness>max_lateness) max_lateness=r.lateness;
        if(r.health==Health::good && r.age>max_age) max_age=r.age;
        if(!has_bad && r.health!=Health::good && !(r.cycle==0 && r.health==Health::warming)) {
            first_bad=r; has_bad=true;
        }
    }
    unsigned retained() const { return cycles<Capacity?cycles:Capacity; }
    const HealthRecord& at(unsigned i) const {
        return tail[(cycles-retained()+i)%Capacity];
    }
};
} // namespace dmotor_monitor
