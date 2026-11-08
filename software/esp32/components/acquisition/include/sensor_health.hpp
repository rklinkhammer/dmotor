#pragma once
#include "acquisition.hpp"
#include <cmath>

namespace dmotor_monitor {
// Explicit inhibited configuration, never a discovered population. One address
// represents one physical chip; duplicate addresses cannot qualify four motors.
struct SensorConfig {
    unsigned population = 0;
    std::array<uint8_t,4> address{{0x40,0x41,0x42,0x43}};
    std::array<uint16_t,4> configuration{{0,0,0,0}};
    // Conservative maximum duration of a conversion/averaging window, including
    // oscillator tolerance, supplied from reviewed configuration (not nominal).
    std::array<Us,4> conversion_bound_us{{0,0,0,0}};
    bool valid() const {
        if(!population || population>15) return false;
        for(unsigned i=0;i<4;++i) if(population & (1u<<i)) {
            constexpr Us times[]={140,204,332,588,1100,2116,4156,8244};
            constexpr Us averages[]={1,4,16,64,128,256,512,1024};
            const auto bits=configuration[i];
            const Us nominal=(times[(bits>>3)&7]+times[(bits>>6)&7])*averages[(bits>>9)&7];
            if(conversion_bound_us[i]<nominal) return false;
            if(address[i]<0x40 || address[i]>0x4f ||
               (configuration[i]&0x8000) || (configuration[i]&7)!=7 ||
               conversion_bound_us[i]<=0 || conversion_bound_us[i]>=25000) return false;
            for(unsigned j=0;j<i;++j)
                if((population & (1u<<j)) && address[i]==address[j]) return false;
        }
        return true;
    }
};
enum class Health { good, config, identity, io, warming, not_ready, stale, overflow, clock };
struct PhysicalSample {
    bool transport=false;
    uint16_t manufacturer=0, die=0, configuration=0, mask=0, shunt=0, bus=0;
    Us mask_started=0, mask_finished=0, finished=0;
};
struct Measurement {
    Health health=Health::warming;
    int32_t shunt_raw=0;
    uint16_t bus_raw=0;
    Us oldest_possible_us=0;
};
class Freshness {
    SensorConfig config_;
    std::array<Us,4> last_clear_{{-1,-1,-1,-1}};
    std::array<Us,4> last_observed_{{-1,-1,-1,-1}};
public:
    explicit Freshness(SensorConfig c) : config_(c) {}
    Measurement observe(unsigned device, const PhysicalSample& s, Us now) {
        Measurement out;
        auto fail=[&](Health h) { out.health=h; return out; };
        if(!config_.valid() || device>=4 || !(config_.population&(1u<<device)))
            return fail(Health::config);
        auto& clear=last_clear_[device];
        if(now<last_observed_[device]) { clear=-1; return fail(Health::clock); }
        last_observed_[device]=now;
        // Any uncertain transaction invalidates clear history, including a mask
        // read that may have reached the device despite a transport error.
        if(!s.transport) { clear=-1; return fail(Health::io); }
        if(s.manufacturer!=0x5449 || (s.die&0xfff0)!=0x2260) {
            clear=-1; return fail(Health::identity);
        }
        if(s.configuration!=config_.configuration[device]) {
            clear=-1; return fail(Health::config);
        }
        if(s.mask_started<0 || s.mask_finished<s.mask_started ||
           s.finished<s.mask_finished || now<s.finished ||
           (clear>=0 && s.mask_started<clear)) {
            clear=-1; return fail(Health::clock);
        }
        const Us previous=clear;
        // Mask read clears CVRF. Read start is conservative: the last completed
        // conversion occurred after the preceding clear, but its window began
        // up to conversion_bound_us earlier. No equality of raw values assumed.
        clear=s.mask_started;
        if(previous<0) return fail(Health::warming);
        if(!(s.mask&8)) return fail(Health::not_ready);
        if(s.mask&4) return fail(Health::overflow);
        const Us bound=config_.conversion_bound_us[device];
        if(previous<bound || now-previous>=25000-bound) return fail(Health::stale);
        out.oldest_possible_us=previous-bound;
        out.shunt_raw=s.shunt<0x8000 ? s.shunt : int32_t(s.shunt)-65536;
        out.bus_raw=s.bus; out.health=Health::good;
        return out;
    }
};
// Versioned canonical calibration record; CRC is integrity, not authenticity.
// Channels 0..3: shunt raw->mA; 4..7: bus raw->mV; 8..9: both battery ADCs->mV.
// Integer coefficients avoid NaN encodings and host structure padding in CRC.
struct Calibration {
    uint32_t version=1, generation=0, board=0, population=0;
    std::array<int32_t,10> gain_ppm{};
    std::array<int32_t,10> offset_milli{};
    uint32_t checksum=0;
    uint32_t crc() const {
        uint32_t c=0xffffffffu;
        auto word=[&](uint32_t w) {
            for(unsigned b=0;b<4;++b) {
                c^=(w>>(8*b))&255u;
                for(unsigned k=0;k<8;++k) c=(c>>1)^((c&1)?0xedb88320u:0u);
            }
        };
        word(version); word(generation); word(board); word(population);
        for(auto v:gain_ppm) word(static_cast<uint32_t>(v));
        for(auto v:offset_milli) word(static_cast<uint32_t>(v));
        return ~c;
    }
    bool valid(uint32_t expected_board, unsigned installed) const {
        if(version!=1 || !generation || !board || board!=expected_board ||
           !installed || installed>15 || population!=installed || checksum!=crc()) return false;
        for(unsigned i=0;i<10;++i) {
            const bool required=i>=8 || (installed & (1u<<(i%4)));
            if(required && gain_ppm[i]<=0) return false;
        }
        return true;
    }
    bool convert(unsigned channel,int32_t raw,uint32_t expected_board,unsigned installed,
                 double& value) const {
        value=0;
        if(channel>=10 || !valid(expected_board,installed) ||
           (channel<8 && !(installed&(1u<<(channel%4))))) return false;
        value=double(raw)*gain_ppm[channel]/1000000.0 + offset_milli[channel]/1000.0;
        return std::isfinite(value);
    }
};
} // namespace dmotor_monitor
