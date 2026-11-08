#pragma once
#include "sensor_health.hpp"
namespace dmotor_monitor {
struct PhysicalResult {
    std::array<Measurement,4> device{};
    unsigned valid=0;
    bool complete=false;
};
// Single I2C owner. Identity/config are checked each cycle, not cached across a
// possible sensor reset. This deliberately adds bus work that must be measured.
class PhysicalAcquisition {
    Clock& clock_; Bus& bus_; SensorConfig config_; Freshness freshness_;
public:
    PhysicalAcquisition(Clock& clock, Bus& bus, SensorConfig c)
        : clock_(clock), bus_(bus), config_(c), freshness_(c) {}
    PhysicalResult run(Us release, Us end_offset=acquisition_deadline_us) {
        PhysicalResult out;
        if(!config_.valid() || end_offset<=0 || end_offset>acquisition_deadline_us || release<0 ||
           release>std::numeric_limits<Us>::max()-acquisition_deadline_us) return out;
        const Us deadline=release+end_offset;
        bool transport=true;
        for(unsigned d=0;d<4;++d) if(config_.population&(1u<<d)) {
            PhysicalSample s;
            Us previous=release;
            auto read=[&](uint8_t reg,uint16_t& value) {
                const Us before=clock_.now();
                if(before<previous || before>=deadline) return false;
                const int wait=timeout_ms(deadline-before,1000);
                if(!wait) return false;
                const bool ok=bus_.read(d,reg,wait,value);
                const Us after=clock_.now(); previous=after;
                return ok && after>=before && after<deadline;
            };
            bool ok=read(0xfe,s.manufacturer) && read(0xff,s.die) && read(0,s.configuration);
            s.mask_started=clock_.now();
            ok=ok && read(6,s.mask); s.mask_finished=clock_.now();
            ok=ok && read(1,s.shunt) && read(2,s.bus);
            s.finished=clock_.now(); s.transport=ok;
            out.device[d]=freshness_.observe(d,s,s.finished);
            if(out.device[d].health==Health::good) out.valid|=1u<<d;
            transport=transport && ok;
        }
        out.complete=transport;
        return out;
    }
};
} // namespace dmotor_monitor
