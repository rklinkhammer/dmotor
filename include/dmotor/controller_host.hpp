#pragma once
#include "dmotor/controller_registers.hpp"
#include "dmotor/batch_host.hpp"
namespace dmotor {
// Single owner. Io supplies aligned device read/write and a device-ordering fence.
// No implicit polling, enable, watchdog refresh or protective acknowledgment.
template<class Io> bool begin_controller_session(Io& io, uint32_t session) {
    using namespace controller_reg;
    if (!session || io.read(IDENTITY)!=PROFILE_ID || (io.read(STATUS)&15U)) return false;
    if (io.read(SESSION) && !io.write(CORE_RESET,1)) return false;
    io.fence();
    return io.write(SESSION,session);
}
template<class Io> bool configure_motor(Io& io, unsigned motor, const uint32_t (&config)[20]) {
    using namespace controller_reg;
    if(motor>=4 || !config[0]) return false;
    const uint32_t base=CONFIG_BASE+motor*CONFIG_STRIDE;
    for(unsigned word=0;word<20;++word)
        if(!io.write(base+4*word,config[word]))return false;
    io.fence();
    const bool okay=io.write(base+CONFIG_COMMIT,1);
    io.fence();
    return okay;
}
template<class Io> bool refresh_controller_watchdog(Io& io, uint32_t session,
                                                   unsigned motor,uint32_t sequence) {
    using namespace controller_reg;
    if(!session || motor>=4 || !sequence)return false;
    if(!io.write(REFRESH_SESSION,session))return false;
    io.fence();
    return io.write(REFRESH_BASE+motor*4,sequence);
}
struct MotorTelemetrySnapshot { uint32_t sequence=0;uint32_t words[4][21] = {}; };
template<class Io> bool capture_motor_telemetry(Io& io, MotorTelemetrySnapshot& output) {
    using namespace controller_reg;
    if(!io.write(SNAPSHOT,1))return false;
    io.fence();
    MotorTelemetrySnapshot result;
    result.sequence=io.read(SNAPSHOT);
    for(unsigned motor=0;motor<4;++motor)for(unsigned word=0;word<21;++word)
        result.words[motor][word]=io.read(TELEMETRY_BASE+motor*TELEMETRY_STRIDE+word*4);
    io.fence();
    if(io.read(SNAPSHOT)!=result.sequence)return false;
    output=result;return true;
}
}
