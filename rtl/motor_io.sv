// Actual atomic-mailbox HLS IP + scheduler/target age/PWM. Motor engine ports
// remain normalized until the shared C++ engine adapter is integrated.
module dmotor_motor_io #(
    parameter integer SNAPSHOT_BITS=256,
    parameter integer TICK_CYCLES=dmotor_timing::CONTROL_TICK_CYCLES,
    parameter integer DEADLINE_CYCLES=dmotor_timing::CONTROL_DEADLINE_CYCLES,
    parameter integer PWM_CYCLES=dmotor_timing::PWM_PERIOD_CYCLES
)(
    input wire clk, reset, inhibit, cancel_frame,
    input wire [3:0] direct_mode,
    input wire signed [31:0] direct_duty,
    input wire direct_valid,
    input wire halt_async, core_reset, acknowledge_local, acknowledge_external, acknowledge_halt,
    input wire [3:0] qa,qb,physical_faults,enable_request,disable_request,refresh_valid,encoder_initialize,
    input wire [127:0] refresh_sequences,
    input wire [31:0] refresh_session,
    input wire [63:0] watchdog_timeout_cycles,
    output wire [255:0] positions,watchdog_fault_times,
    output wire [127:0] watchdog_faults,
    output wire shared_halt,
    output wire target_inhibit,
    output wire [3:0] enabled,
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
    wire [1023:0] snapshots;
    wire [3:0] safety_requests,external_latches,halt_detected,decoder_fault,safety_inhibits;
    (* ASYNC_REG = "TRUE" *) reg [3:0] physical_meta,physical_sync;
    // One CDC chain for the common HALT input, before fan-out to motor safety.
    // Preserve the chain across MMIO core_reset, like the protective latches.
    (* ASYNC_REG = "TRUE", DONT_TOUCH = "TRUE" *) reg halt_meta,halt_sync;
    always @(posedge clk)begin
        if(reset)begin halt_meta<=0;halt_sync<=0;end
        else begin halt_meta<=halt_async;halt_sync<=halt_meta;end
    end
    wire ctrl_fault,ctrl_halt,aggregated,global_inhibit;
    wire direct_active=|direct_mode;
    wire engine_fault,engine_halt,engine_initialized,engine_busy,engine_blocked;
    wire [3:0] engine_ready_mask,engine_stopped,engine_age_faults;
    wire [3:0] engine_pwm,engine_enable,engine_direction;
    wire [127:0] engine_applied;
    wire [2047:0] engine_telemetry;
    reg direct_fault,direct_armed;
    reg [31:0] direct_age;
    reg signed [31:0] direct_requested;
    wire direct_pwm,direct_enable,direct_direction;
    wire signed [31:0] direct_applied;
    wire direct_running=|(enabled & direct_mode);
    assign ctrl_fault=engine_fault || direct_fault;
    assign ctrl_halt=engine_halt || direct_fault;
    assign protocol_initialized=direct_active || engine_initialized;
    assign ready_mask=direct_active ? (enabled & direct_mode & ~stop) : engine_ready_mask;
    assign stopped=direct_active ? stop : engine_stopped;
    assign busy=direct_active ? 1'b0 : engine_busy;
    assign fault_inhibit=direct_active ? (core_reset || global_inhibit || direct_fault) : engine_blocked;
    assign age_faults=direct_active ? ({4{direct_fault}} & direct_mode) : engine_age_faults;
    assign pwm=direct_active ? ({4{direct_pwm}} & direct_mode) : engine_pwm;
    assign drive_enable=direct_active ? ({4{direct_enable}} & direct_mode) : engine_enable;
    assign direction=direct_active ? ({4{direct_direction}} & direct_mode) : engine_direction;
    genvar dm;
    generate for(dm=0;dm<4;dm=dm+1)begin: direct_feedback
      assign applied_duties[dm*32 +:32]=direct_active ? (direct_mode[dm] ? direct_applied : 32'd0) : engine_applied[dm*32 +:32];
      assign telemetry[dm*512 +:512]=direct_active ? {352'd0,(direct_mode[dm] ? direct_applied : 32'd0),128'd0} : engine_telemetry[dm*512 +:512];
    end endgenerate
    always @(posedge clk)begin
      if(reset || core_reset || !direct_active)begin direct_fault<=0;direct_armed<=0;direct_age<=0;direct_requested<=0;end
      else if(!direct_running || global_inhibit || |(stop & direct_mode))begin direct_armed<=0;direct_age<=0;direct_requested<=0;end
      else if(direct_armed && direct_age>=age_timeout_cycles-1)begin direct_fault<=1;direct_requested<=0;end
      else if(direct_valid)begin direct_requested<=direct_duty;direct_armed<=1;direct_age<=0;end
      else if(direct_armed)direct_age<=direct_age+1'b1;
    end
    dmotor_pwm #(.PERIOD_CYCLES(PWM_CYCLES)) direct_output(.clk(clk),.reset(reset || core_reset || !direct_active),
      .inhibit(global_inhibit || direct_fault || !direct_running || |(stop & direct_mode)),
      .requested_duty_q16(direct_requested),.pwm(direct_pwm),.drive_enable(direct_enable),
      .direction(direct_direction),.applied_duty_q16(direct_applied),.reversing(),.timestamp(),.application_timestamp());
    assign global_inhibit=inhibit || aggregated || |halt_detected || |external_latches || |physical_sync || ctrl_fault || |(enabled & safety_inhibits);
    assign shared_halt=aggregated || |safety_requests || ctrl_halt;
    assign fault=ctrl_fault || |watchdog_faults;
    assign halt_request=shared_halt;
    always @(posedge clk)begin
        if(reset)begin physical_meta<=0;physical_sync<=0;end
        else begin physical_meta<=physical_faults;physical_sync<=physical_meta;end
    end
    dmotor_halt_aggregator #(.MOTORS(5)) aggregate(.clk(clk),.reset(reset),.acknowledge(acknowledge_halt),
        .requests({ctrl_halt,safety_requests}),.halted(aggregated));
    genvar m;
    generate for(m=0;m<4;m=m+1)begin: channel
        wire event_valid,event_negative,negative,capture_fault;
        wire [63:0] event_timestamp,encoder_time,count,last_edge;
        wire [31:0] invalid,epoch,snapshot_invalid;
        reg stop_seen;
        reg [63:0] stop_started;
        wire [63:0] stop_budget={32'd0,configurations[m*640+13*32 +:32]}*dmotor_timing::CLOCKS_PER_US;
        wire stop_expired=enabled[m] && stop_seen && !stopped[m] && encoder_time-stop_started>=stop_budget;
        always @(posedge clk)begin
            if(reset || !enabled[m])begin stop_seen<=0;stop_started<=0;end
            else if(stop[m] && !stop_seen)begin stop_seen<=1;stop_started<=encoder_time;end
        end

        qcencoder decoder(.clk(clk),.reset(reset || core_reset),.initialize(encoder_initialize[m]),
            .qa(qa[m]),.qb(qb[m]),.invert(1'b0),.mode(2'd2),.initial_position(64'sd0),
            .position(positions[m*64 +:64]),.invalid_transitions(invalid),.range_fault(decoder_fault[m]),
            .event_valid(event_valid),.event_negative(event_negative),.timestamp(encoder_time),.event_timestamp(event_timestamp));
        dmotor_encoder_snapshot capture(.clk(clk),.reset(reset || core_reset || encoder_initialize[m]),
            .event_valid(event_valid),.event_negative(event_negative),.event_timestamp(event_timestamp),
            .encoder_invalid_transitions(invalid),.encoder_range_fault(decoder_fault[m]),
            .count(count),.last_edge(last_edge),.direction_epoch(epoch),.invalid_transitions(snapshot_invalid),
            .negative(negative),.capture_fault(capture_fault));
        assign snapshots[m*256 +:256]={62'd0,capture_fault,negative,snapshot_invalid,epoch,last_edge,count};
        dmotor_safety #(.HALT_ALREADY_SYNCHRONIZED(1)) safety(.clk(clk),.reset(reset),.halt_async(halt_sync),
            .local_faults({27'd0,stop_expired,decoder_fault[m],physical_sync[m],ctrl_fault,1'b0}),
            .enable_request(enable_request[m]),.disable_request(disable_request[m] || global_inhibit),
            .prerequisites_valid(authorized[m] && protocol_initialized && !global_inhibit),
            .acknowledge_local(acknowledge_local && !enabled[m]),.acknowledge_external(acknowledge_external && !enabled[m]),.acknowledge_mask(32'hffffffff),
            .timeout_cycles(watchdog_timeout_cycles),.refresh_valid(refresh_valid[m]),
            .session(session),.refresh_session(refresh_session),.refresh_sequence(refresh_sequences[m*32 +:32]),
            .enabled(enabled[m]),.inhibit(safety_inhibits[m]),.halt_out(safety_requests[m]),
            .external_latched(external_latches[m]),.faults(watchdog_faults[m*32 +:32]),
            .expected_sequence(),.timestamp(),.last_refresh(),.first_fault_timestamp(watchdog_fault_times[m*64 +:64]),
            .halt_detected(halt_detected[m]));
    end endgenerate
    dmotor_integrated_control #(.SNAPSHOT_BITS(SNAPSHOT_BITS),.TICK_CYCLES(TICK_CYCLES),
        .DEADLINE_CYCLES(DEADLINE_CYCLES),.PWM_CYCLES(PWM_CYCLES)) controller(
        .reset(reset || core_reset || direct_active),.inhibit(global_inhibit || |safety_requests || core_reset || direct_active),
        .fault(engine_fault),.halt_request(engine_halt),.protocol_initialized(engine_initialized),
        .ready_mask(engine_ready_mask),.stopped(engine_stopped),.busy(engine_busy),.fault_inhibit(engine_blocked),
        .age_faults(engine_age_faults),.pwm(engine_pwm),.drive_enable(engine_enable),.direction(engine_direction),
        .applied_duties(engine_applied),.telemetry(engine_telemetry),.*);
endmodule
