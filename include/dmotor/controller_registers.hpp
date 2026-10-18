#pragma once
#include <cstdint>
namespace dmotor { namespace controller_reg {
constexpr uint32_t PROFILE_ID = 1145915698U;
constexpr uint32_t IDENTITY = 4096U;
constexpr uint32_t STATUS = 4100U;
constexpr uint32_t SESSION = 4104U;
constexpr uint32_t ENABLE = 4108U;
constexpr uint32_t DISABLE = 4112U;
constexpr uint32_t STOP = 4116U;
constexpr uint32_t REFRESH_SESSION = 4120U;
constexpr uint32_t CORE_RESET = 4128U;
constexpr uint32_t ACKNOWLEDGE = 4132U;
constexpr uint32_t WATCHDOG_CYCLES = 4136U;
constexpr uint32_t TARGET_TIMEOUT_CYCLES = 4140U;
constexpr uint32_t SNAPSHOT = 4144U;
constexpr uint32_t REFRESH_BASE = 4160U;
constexpr uint32_t CONFIG_BASE = 8192U;
constexpr uint32_t CONFIG_STRIDE = 256U;
constexpr uint32_t CONFIG_COMMIT = 128U;
constexpr uint32_t TELEMETRY_BASE = 12288U;
constexpr uint32_t TELEMETRY_STRIDE = 84U;
constexpr uint32_t DIRECT_CAPABILITY = 4176U;
constexpr uint32_t DIRECT_MODE = 4180U;
constexpr uint32_t DIRECT_LIMIT = 4184U;
constexpr uint32_t DIRECT_DUTY = 4188U;
constexpr uint32_t DIRECT_COMMIT = 4192U;
} }
