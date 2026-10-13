// Actual atomic-mailbox HLS IP + scheduler/target age/PWM. Motor engine ports
// remain normalized until the shared C++ engine adapter is integrated.
module dmotor_integrated_control #(
    parameter integer SNAPSHOT_BITS=256,
    parameter integer TICK_CYCLES=dmotor_timing::CONTROL_TICK_CYCLES,
    parameter integer DEADLINE_CYCLES=dmotor_timing::CONTROL_DEADLINE_CYCLES,
    parameter integer PWM_CYCLES=dmotor_timing::PWM_PERIOD_CYCLES
)(
    input wire clk, reset, inhibit, cancel_frame,
    output wire target_inhibit,
    input wire [3:0] enabled,
    input wire [3:0] authorized, invalidate_mask,
    input wire [127:0] max_speeds,
    input wire [2559:0] configurations,
    input wire [3:0] stop,
    output wire [3:0] ready_mask, stopped,
    output wire [2047:0] telemetry,
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
    wire engine_start,engine_ready,engine_done,engine_fault,engine_reset,engine_apply_target;
    wire [1:0] engine_motor,result_motor;
    wire [31:0] engine_session,engine_generation,engine_target,result_session,result_generation;
    wire [SNAPSHOT_BITS-1:0] engine_snapshot;
    wire [63:0] result_tick;
    wire signed [31:0] result_duty;
    wire controller_fault, controller_halt;
    wire [2047:0] engine_telemetry;
    reg [2047:0] retained_telemetry;
    assign telemetry=retained_telemetry;
    always @(posedge clk)begin
        if(reset)retained_telemetry<=0;
        else if(!engine_reset || engine_fault)retained_telemetry<=engine_telemetry;
    end
    assign fault=controller_fault || engine_fault;
    assign halt_request=controller_halt || engine_fault;
    dmotor_motor_engine_adapter engine(.clk(clk),.reset(reset || engine_reset),
        .start(engine_start),.ready(engine_ready),.done(engine_done),.fault(engine_fault),
        .motor(engine_motor),.session(engine_session),.generation(engine_generation),
        .target(engine_target),.tick(frame_tick),.snapshot(engine_snapshot),
        .apply_target(engine_apply_target),.enabled(enabled),.stop(stop),.configurations(configurations),
        .ready_mask(ready_mask),.stopped(stopped),.telemetry(engine_telemetry),
        .result_motor(result_motor),.result_session(result_session),.result_generation(result_generation),
        .result_tick(result_tick),.result_duty(result_duty));
    dmotor_mailbox_control #(.SNAPSHOT_BITS(SNAPSHOT_BITS),.TICK_CYCLES(TICK_CYCLES),
        .DEADLINE_CYCLES(DEADLINE_CYCLES),.PWM_CYCLES(PWM_CYCLES)) control(
        .inhibit(inhibit),.fault(controller_fault),.halt_request(controller_halt),.*);
endmodule
