#pragma once
#include <cstdint>
namespace dmotor_sw {
// Local SOCK_SEQPACKET protocol; fixed width, no pointers/padding-dependent payload.
constexpr uint32_t service_schema = 1;
constexpr unsigned service_period_ms = 5;
constexpr unsigned client_ping_ms = 100;
enum class Operation : uint32_t {
    ping,
    status,
    prepare,
    enable,
    velocity,
    stop,
    disable,
    halt,
    quit,
    inject_target,
    inject_watchdog,
    duty,
    recovery_status
};
struct Request {
    uint32_t schema = service_schema;
    Operation operation = Operation::status;
    uint32_t mask = 0;
    int32_t velocity[4] = {};
};
struct Response {
    uint32_t schema = service_schema;
    uint32_t okay = 0;
    uint32_t status = 0;
    uint32_t applied = 0;
    int32_t measured[4] = {};
    uint32_t position_lo[4] = {}, position_hi[4] = {};
    char message[128] = {};
};
// Schema 2 is currently a read-only recovery query using the same Request shape.
// Legacy commands remain schema 1. Never reinterpret PL status as monitor admission.
constexpr uint32_t recovery_service_schema = 2;
struct RecoveryResponse {
    Response base{};
    uint32_t recovery_state = 0; // RecoveryState, unavailable by default.
    uint32_t admission_open = 0;
};
static_assert(sizeof(RecoveryResponse) == 200, "Unexpected recovery response ABI");
static_assert(sizeof(Request) == 28, "Unexpected request ABI");
static_assert(sizeof(Response) == 192, "Unexpected response ABI");
} // namespace dmotor_sw
