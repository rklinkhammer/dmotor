// Integration boundary for normalized HLS/protocol handshakes, independent
// target-age protection and four phase-aligned PWM channels. Authorization
// watchdogs and physical fault qualification feed inhibit externally.
module dmotor_shared_control #(
    parameter integer SNAPSHOT_BITS=256,
    parameter integer TICK_CYCLES=dmotor_timing::CONTROL_TICK_CYCLES,
    parameter integer DEADLINE_CYCLES=dmotor_timing::CONTROL_DEADLINE_CYCLES,
    parameter integer PWM_CYCLES=dmotor_timing::PWM_PERIOD_CYCLES
)(
    input wire clk, reset, inhibit, cancel_frame,
    output wire target_inhibit,
    input wire [3:0] enabled,
    input wire [31:0] session,
    input wire [127:0] generations,
    input wire [4*SNAPSHOT_BITS-1:0] snapshots,
    output wire [63:0] timestamp, frame_tick,
    output wire admission_request,
    input wire admission_done, batch_valid,
    input wire [3:0] batch_mask,
    input wire [31:0] batch_session, batch_sequence,
    input wire [127:0] batch_generations, batch_targets,
    output wire engine_start,
    input wire engine_ready, engine_done, engine_fault,
    output wire engine_reset,
    output wire [1:0] engine_motor,
    output wire [31:0] engine_session, engine_generation,
    output wire [SNAPSHOT_BITS-1:0] engine_snapshot,
    output wire engine_apply_target,
    output wire [31:0] engine_target,
    input wire [1:0] result_motor,
    input wire [63:0] result_tick,
    input wire [31:0] result_session, result_generation,
    input wire signed [31:0] result_duty,
    output wire target_apply,
    output wire [3:0] target_mask,
    output wire target_accepted,
    output wire retire_request,
    output wire [31:0] retire_session, retire_sequence,
    input wire retire_done, retire_success,
    output wire frame_aborted, frame_committed,
    output wire [127:0] committed_duties,
    output wire [127:0] pwm_duties,
    output wire busy, fault_inhibit,
    output wire fault,
    output wire [31:0] last_latency, maximum_latency,
    input wire [3:0] age_configure, age_acknowledge, stop_or_ready,
    input wire [31:0] age_timeout_cycles,
    output wire [3:0] age_configured, age_configure_accepted, age_armed, age_faults,
    output wire [255:0] target_ages, target_ticks, target_fault_times,
    output wire halt_request,
    output wire [3:0] pwm, drive_enable, direction,
    output wire [127:0] applied_duties
);
    wire age_halt;
    dmotor_shared_scheduler #(.SNAPSHOT_BITS(SNAPSHOT_BITS),.TICK_CYCLES(TICK_CYCLES),
        .DEADLINE_CYCLES(DEADLINE_CYCLES),.PWM_CYCLES(PWM_CYCLES)) scheduler(.*);
    dmotor_target_age target_age(.clk(clk),.reset(reset),.timestamp(timestamp),
        .enabled(enabled),.stop_or_ready(stop_or_ready),.configure(age_configure),
        .acknowledge(age_acknowledge),.timeout_cycles(age_timeout_cycles),
        .applied_valid(target_apply),.applied_mask(target_mask),.applied_tick(frame_tick),
        .configured(age_configured),.armed(age_armed),.faults(age_faults),
        .configure_accepted(age_configure_accepted),.applied_accepted(target_accepted),
        .inhibit_all(target_inhibit),.halt_out(age_halt),.expired_now(),
        .age_cycles(target_ages),.last_applied_ticks(target_ticks),.fault_timestamps(target_fault_times));
    assign halt_request=fault || age_halt;
    genvar g;
    generate for(g=0;g<4;g=g+1)begin: channels
        dmotor_pwm #(.PERIOD_CYCLES(PWM_CYCLES)) output_stage(.clk(clk),.reset(reset),
            .inhibit(fault_inhibit || !enabled[g]),.requested_duty_q16(pwm_duties[g*32 +:32]),
            .pwm(pwm[g]),.drive_enable(drive_enable[g]),.direction(direction[g]),
            .reversing(),.applied_duty_q16(applied_duties[g*32 +:32]),
            .timestamp(),.application_timestamp());
    end endgenerate
endmodule
