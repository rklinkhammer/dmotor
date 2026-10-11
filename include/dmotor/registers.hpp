// Generated; edit abi/registers.json or include/dmotor/ring_config.hpp.
#pragma once
#include <cstdint>
#include "dmotor/ring_config.hpp"
namespace dmotor { namespace reg {
constexpr uint32_t IDENTITY = 0x0000;
constexpr uint32_t SCHEMA_VERSION = 0x0004;
constexpr uint32_t CAPABILITIES = 0x0008;
constexpr uint32_t CLOCK_HZ = 0x000c;
constexpr uint32_t STATUS = 0x0010;
constexpr uint32_t ENABLE = 0x0020;
constexpr uint32_t DISABLE = 0x0024;
constexpr uint32_t STOP = 0x0028;
constexpr uint32_t FAULTS = 0x0030;
constexpr uint32_t ACK_LOCAL = 0x0034;
constexpr uint32_t ACK_EXTERNAL = 0x0038;
constexpr uint32_t WATCHDOG_US = 0x0040;
constexpr uint32_t REFRESH_SESSION = 0x0044;
constexpr uint32_t REFRESH_SEQUENCE = 0x0048;
constexpr uint32_t EXPECTED_REFRESH = 0x004c;
constexpr uint32_t QUEUE_EPOCH = 0x0100;
constexpr uint32_t QUEUE_PRODUCER = 0x0104;
constexpr uint32_t QUEUE_CONSUMER = 0x0108;
constexpr uint32_t PUBLISH_SESSION = 0x010c;
constexpr uint32_t PUBLISH = 0x0110;
constexpr uint32_t COMPLETION_PRODUCER = 0x0114;
constexpr uint32_t COMPLETION_ACK = 0x0118;
constexpr uint32_t QUIESCE = 0x011c;
constexpr uint32_t REOPEN = 0x0120;
constexpr uint32_t CANCEL_ACK = 0x0124;
constexpr uint32_t COMPLETION_CONSUMER = 0x0128;
constexpr uint32_t CONFIG_COMMIT = 0x0050;
constexpr uint32_t MAX_SPEED_MRPM = 0x0054;
constexpr uint32_t CONFIG_GENERATION = 0x0058;
constexpr uint32_t ACTIVE_WATCHDOG_US = 0x005c;
constexpr uint32_t ACTIVE_MAX_SPEED_MRPM = 0x0060;
constexpr uint32_t SAFETY_STATUS = 0x0064;
constexpr uint32_t SUBMISSION_OFFSET = 4096;
constexpr uint32_t SUBMISSION_SLOTS = dmotor::ring_depth;
constexpr uint32_t SUBMISSION_STRIDE = 64;
constexpr uint32_t COMPLETION_OFFSET = 8192;
constexpr uint32_t COMPLETION_SLOTS = dmotor::ring_depth;
constexpr uint32_t COMPLETION_STRIDE = 32;
constexpr uint32_t ACTIVE_OFFSET = 12288;
constexpr uint32_t ACTIVE_SLOTS = 1;
constexpr uint32_t ACTIVE_STRIDE = 64;
constexpr uint32_t CANCELLATION_OFFSET = 12544;
constexpr uint32_t CANCELLATION_SLOTS = 1;
constexpr uint32_t CANCELLATION_STRIDE = 32;
constexpr uint32_t ABI_SCHEMA = 4;
constexpr uint32_t FABRIC_HZ = 100000000;
} } // namespace dmotor::reg
