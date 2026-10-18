#pragma once
#include <cstdint>
namespace dmotor_sw {
// Local SOCK_SEQPACKET protocol; fixed width, no pointers/padding-dependent payload.
constexpr uint32_t service_schema=1;
constexpr unsigned service_period_ms=5;
constexpr unsigned client_ping_ms=100;
enum class Operation : uint32_t { ping, status, prepare, enable, velocity, stop, disable, halt, quit, inject_target, inject_watchdog, duty };
struct Request {
    uint32_t schema=service_schema;
    Operation operation=Operation::status;
    uint32_t mask=0;
    int32_t velocity[4]={};
};
struct Response {
    uint32_t schema=service_schema;
    uint32_t okay=0;
    uint32_t status=0;
    uint32_t applied=0;
    int32_t measured[4]={};
    uint32_t position_lo[4]={},position_hi[4]={};
    char message[128]={};
};
static_assert(sizeof(Request)==28,"Unexpected request ABI");
static_assert(sizeof(Response)==192,"Unexpected response ABI");
}
