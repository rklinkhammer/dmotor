#pragma once
#include <cstdint>
// One hardware transaction. The AXI adapter owns ap_start/ap_done; idle
// transactions poll autonomously. No processor start register is exposed.
extern "C" void dmotor_peripheral(
    bool reset, bool bus_valid, bool write, uint32_t address, uint32_t data, uint8_t strobes,
    uint64_t now, bool executor_ready, int64_t position, int32_t max_speed_mrpm,
    bool upstream_drained, uint32_t generation, bool cancel_valid, uint32_t cancel_cause,
    bool completion_valid, uint32_t completion_epoch, uint32_t completion_sequence,
    uint32_t completion_detail, uint32_t& read_data, bool& bus_error, uint32_t& engine_status,
    bool& activated, uint32_t& active_epoch, uint32_t& active_sequence, bool& disable_request,
    bool& stop_request, uint32_t safety_state, uint32_t safety_faults, uint32_t live_faults,
    uint32_t expected_refresh, uint32_t& safety_action, uint32_t& safety_argument,
    uint32_t& refresh_session_out, uint64_t& timeout_cycles, bool& configuration_valid);
