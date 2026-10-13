// Actual atomic-mailbox HLS IP + scheduler/target age/PWM. Motor engine ports
// remain normalized until the shared C++ engine adapter is integrated.
module dmotor_mailbox_control #(
    parameter integer SNAPSHOT_BITS=256,
    parameter integer TICK_CYCLES=dmotor_timing::CONTROL_TICK_CYCLES,
    parameter integer DEADLINE_CYCLES=dmotor_timing::CONTROL_DEADLINE_CYCLES,
    parameter integer PWM_CYCLES=dmotor_timing::PWM_PERIOD_CYCLES
)(
    input wire clk, reset, inhibit, cancel_frame,
    output wire target_inhibit,
    input wire [3:0] enabled,
    input wire [3:0] authorized, ready_mask, invalidate_mask,
    input wire [127:0] max_speeds,
    output wire protocol_initialized, protocol_fault,
    input wire host_valid, host_write,
    input wire [31:0] host_address, host_wdata,
    input wire [3:0] host_strobes,
    output wire host_ready, host_response_valid,
    input wire host_response_ready,
    output wire host_response_okay,
    output wire [31:0] host_rdata,
    input wire [31:0] session,
    input wire [127:0] generations,
    input wire [4*SNAPSHOT_BITS-1:0] snapshots,
    output wire [63:0] timestamp, frame_tick,
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
    wire admission_request, admission_done, batch_valid, retire_request, retire_done, retire_success;
    wire [3:0] batch_mask;
    wire [31:0] batch_session, batch_sequence, retire_session, retire_sequence;
    wire [127:0] batch_generations, batch_targets;
    wire cancellation_pending;
    reg frame_has_engine;
    always @(posedge clk)begin
        if(reset || !busy)frame_has_engine<=0;
        else if(engine_start && engine_ready)frame_has_engine<=1;
    end
    wire [351:0] live_admission={max_speeds,generations,28'd0,ready_mask,
                                28'd0,(authorized & enabled),session};
    wire [3:0] protocol_invalidate=invalidate_mask |
        ((frame_aborted || cancel_frame) ? 4'hf : 4'h0);
    wire control_fault, control_halt;
    assign fault=control_fault || protocol_fault;
    assign halt_request=control_halt || protocol_fault;
    dmotor_atomic_mailbox_arbiter protocol(
        .live_admission(live_admission),.invalidate_mask(protocol_invalidate),
        .initialized(protocol_initialized),.fault(protocol_fault),.quiescent(),
        .inhibit(inhibit || target_inhibit || control_fault),.*);
    dmotor_shared_control #(.SNAPSHOT_BITS(SNAPSHOT_BITS),.TICK_CYCLES(TICK_CYCLES),
        .DEADLINE_CYCLES(DEADLINE_CYCLES),.PWM_CYCLES(PWM_CYCLES)) control(
        .inhibit(inhibit || protocol_fault || !protocol_initialized),
        .cancel_frame(cancel_frame || (frame_has_engine && (cancellation_pending || |invalidate_mask))),
        .fault(control_fault),.halt_request(control_halt),.*);
endmodule
