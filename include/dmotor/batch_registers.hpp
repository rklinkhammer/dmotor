// Generated from abi/mailbox.json; provisional, separate from ring ABI.
#pragma once
#include <cstdint>
namespace dmotor { namespace batch_reg {
constexpr uint32_t PROFILE_ID = 1145913905U;
constexpr uint32_t SCHEMA_VERSION = 1U;
constexpr uint32_t CHANNELS = 4U;
constexpr uint32_t STAGE_MOTOR_BASE = 128U;
constexpr uint32_t STAGE_MOTOR_STRIDE = 8U;
constexpr uint32_t APPLIED_MOTOR_BASE = 256U;
constexpr uint32_t APPLIED_MOTOR_STRIDE = 16U;
constexpr uint32_t IDENTITY = 0U;
constexpr uint32_t SCHEMA = 4U;
constexpr uint32_t MOTOR_COUNT = 8U;
constexpr uint32_t SESSION = 12U;
constexpr uint32_t STAGE_SESSION = 16U;
constexpr uint32_t STAGE_SEQUENCE = 20U;
constexpr uint32_t STAGE_MASK = 24U;
constexpr uint32_t COMMIT = 28U;
constexpr uint32_t PUBLISHED_SEQUENCE = 32U;
constexpr uint32_t PENDING_SEQUENCE = 36U;
constexpr uint32_t PENDING_MASK = 40U;
constexpr uint32_t APPLIED_SEQUENCE = 44U;
constexpr uint32_t APPLIED_MASK = 48U;
constexpr uint32_t APPLIED_TICK_LO = 52U;
constexpr uint32_t APPLIED_TICK_HI = 56U;
constexpr uint32_t REJECT_REASON = 60U;
constexpr uint32_t INFLIGHT_SEQUENCE = 64U;
constexpr uint32_t GENERATION = 0U;
constexpr uint32_t VELOCITY_MRPM = 4U;
constexpr uint32_t REJECT_NONE = 0U;
constexpr uint32_t REJECT_ACCESS = 1U;
constexpr uint32_t REJECT_INCOMPLETE = 2U;
constexpr uint32_t REJECT_MASK = 3U;
constexpr uint32_t REJECT_OWNERSHIP = 4U;
constexpr uint32_t REJECT_SEQUENCE = 5U;
constexpr uint32_t REJECT_SPEED = 6U;
constexpr uint32_t REJECT_UNAVAILABLE = 7U;
constexpr uint32_t REJECT_PENDING_MASK = 8U;
constexpr uint32_t REJECT_BUSY = 9U;
constexpr uint32_t REJECT_RETIREMENT = 10U;
} }
