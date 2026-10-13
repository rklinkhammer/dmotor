#include "dmotor/controller_host.hpp"
#include <cassert>
#include <map>
struct Io {
    std::map<uint32_t,uint32_t> words;
    unsigned fences=0,writes=0;
    bool deny=false;
    uint32_t read(uint32_t address) {return words[address];}
    bool write(uint32_t address,uint32_t value) {
        ++writes;if(deny)return false;words[address]=value;return true;
    }
    void fence(){++fences;}
};
int main(){
    using namespace dmotor::controller_reg;
    Io io;io.words[IDENTITY]=PROFILE_ID;
    assert(dmotor::begin_controller_session(io,7));
    uint32_t config[20]={21};
    assert(dmotor::configure_motor(io,3,config));
    assert(io.words[CONFIG_BASE+3*CONFIG_STRIDE]==21);
    assert(io.words[CONFIG_BASE+3*CONFIG_STRIDE+CONFIG_COMMIT]==1);
    const auto writes=io.writes;
    assert(!dmotor::configure_motor(io,4,config));assert(io.writes==writes);
    io.words[STATUS]=1;assert(!dmotor::begin_controller_session(io,8));
    io.deny=true;assert(!dmotor::refresh_controller_watchdog(io,7,0,1));
    io.deny=false;assert(dmotor::refresh_controller_watchdog(io,7,2,4));
    assert(io.words[REFRESH_BASE+8]==4);
    io.words[TELEMETRY_BASE+3*TELEMETRY_STRIDE+80]=99;
    dmotor::MotorTelemetrySnapshot snapshot;
    assert(dmotor::capture_motor_telemetry(io,snapshot));assert(snapshot.words[3][20]==99);
}
