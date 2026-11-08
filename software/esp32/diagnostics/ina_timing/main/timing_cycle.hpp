#pragma once
#include "acquisition.hpp"
namespace dmotor_timing {
using namespace dmotor_monitor;
struct Adc {
    virtual bool read(unsigned channel, int& raw) = 0;
    virtual ~Adc() = default;
};
struct Cycle {
    Result ina{};
    Us started = 0, adc_end = 0, ended = 0;
    unsigned adc_reads = 0;
    bool adc_complete = false;
};
// GPIO1/2 always included. Channels 2..5 optionally represent GPIO4..7.
// One owner. The diagnostic never calls acquisition_qualified or controls outputs.
inline Cycle capture(Clock& clock, Bus& bus, Adc& adc, Us release,
                     unsigned hz, unsigned samples, bool ipropi, bool raw_probe) {
    Cycle c; c.started = clock.now();
    auto fail = [&](Status s) { c.ina.status = s; c.ina.raw = {}; c.ended = clock.now(); return c; };
    if (release < 0 || release > std::numeric_limits<Us>::max()-monitor_period_us ||
        (hz != 100 && hz != 1000) || samples == 0 || samples > 16)
        return fail(Status::config);
    if (c.started < release || c.started >= release+adc_budget_us)
        return fail(Status::deadline);
    Us last = c.started;
    const unsigned channels = ipropi ? 6 : 2;
    for (unsigned ch=0; ch<channels; ++ch) {
        for (unsigned n=0; n<samples; ++n) {
            Us now = clock.now();
            if (now < last || now >= release+adc_budget_us) return fail(Status::deadline);
            int raw=0;
            const bool ok=adc.read(ch,raw);
            const Us after=clock.now();
            if (after < now || after >= release+adc_budget_us) return fail(Status::deadline);
            if (!ok) return fail(Status::io);
            ++c.adc_reads; last=after;
        }
    }
    c.adc_end=clock.now();
    if (c.adc_end < last || c.adc_end >= release+adc_budget_us) return fail(Status::deadline);
    c.adc_complete=true;
    // At most 10 ms INA time, also finish before release+11 ms.
    const Us ina_end=c.adc_end+ina_budget_us;
    if (!raw_probe) {
        Config config; config.tick_hz=hz; config.ina_start_us=0;
        config.ina_end_us=ina_end-release;
        Acquisition acquisition(clock,bus);
        c.ina=acquisition.run(release,config);
    } else {
        c.ina.release=release; c.ina.deadline=ina_end;
        constexpr std::uint8_t regs[]={6,1,2};
        for (unsigned d=0; d<4; ++d) {
            for (unsigned j=0; j<3; ++j) {
                const Us before=clock.now();
                if (before < last || before >= ina_end) return fail(Status::deadline);
                std::uint16_t value=0;
                const bool ok=bus.read(d,regs[j],2,value);
                const Us after=clock.now();
                if (after < before || after >= ina_end) return fail(Status::deadline);
                if (!ok) return fail(Status::io);
                c.ina.raw[d][j]=value; ++c.ina.reads; last=after;
            }
        }
        c.ina.finished=clock.now();
        if (c.ina.finished < last || c.ina.finished >= ina_end)
            return fail(Status::deadline);
        c.ina.status=Status::complete;
    }
    c.ended=clock.now();
    if (c.ended < c.ina.finished || c.ended >= ina_end) {
        c.ina.status=Status::deadline;
    }
    if (c.ina.status != Status::complete) c.ina.raw={};
    return c;
}
} // namespace dmotor_timing
