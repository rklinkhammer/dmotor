// Four-context transaction scheduler. Normalized HLS/protocol interfaces require
// generated-port adapters; this is not an AXI or complete board top.
module dmotor_shared_scheduler #(
    parameter integer SNAPSHOT_BITS=256,
    parameter integer TICK_CYCLES=dmotor_timing::CONTROL_TICK_CYCLES,
    parameter integer DEADLINE_CYCLES=dmotor_timing::CONTROL_DEADLINE_CYCLES,
    parameter integer PWM_CYCLES=dmotor_timing::PWM_PERIOD_CYCLES
)(
    input wire clk, reset, inhibit, target_inhibit, cancel_frame,
    input wire [3:0] enabled,
    input wire [31:0] session,
    input wire [127:0] generations,
    input wire [4*SNAPSHOT_BITS-1:0] snapshots,
    output reg [63:0] timestamp, frame_tick,
    output wire admission_request,
    input wire admission_done, batch_valid,
    input wire [3:0] batch_mask,
    input wire [31:0] batch_session, batch_sequence,
    input wire [127:0] batch_generations, batch_targets,
    output reg engine_start,
    input wire engine_ready, engine_done, engine_fault,
    output wire engine_reset,
    output reg [1:0] engine_motor,
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
    input wire target_accepted,
    output wire retire_request,
    output wire [31:0] retire_session, retire_sequence,
    input wire retire_done, retire_success,
    output reg frame_aborted, frame_committed,
    output reg [127:0] committed_duties,
    output wire [127:0] pwm_duties,
    output wire busy, fault_inhibit,
    output reg fault,
    output reg [31:0] last_latency, maximum_latency
);
    localparam IDLE=0, ADMIT=1, RUN=2, AGE=3, AGE_ACK=4, RETIRE=5, PWM_WAIT=6, DRAIN=7;
    reg [2:0] state;
    reg [31:0] tick_count, pwm_phase, age;
    reg [31:0] saved_session, saved_sequence;
    reg [127:0] saved_generations, saved_targets, staged_duties;
    reg [4*SNAPSHOT_BITS-1:0] saved_snapshots;
    reg [3:0] saved_enabled, saved_mask, remaining;
    reg has_batch, issued;
    wire tick_due=tick_count==TICK_CYCLES-1;
    wire pwm_boundary=pwm_phase==PWM_CYCLES-1;
    wire calculating=state!=IDLE && state!=PWM_WAIT;
    wire deadline=calculating && age>=DEADLINE_CYCLES-1;
    wire overrun=state!=IDLE && tick_due;
    reg tags_changed, bad_batch;
    integer m;
    always @* begin
        tags_changed=session!=saved_session;
        bad_batch=batch_session!=saved_session || batch_sequence==0 || batch_mask==0 ||
                  ((batch_mask & saved_enabled)!=batch_mask);
        for(integer i=0;i<4;i=i+1) begin
            if(saved_enabled[i] && generations[i*32 +:32]!=saved_generations[i*32 +:32]) tags_changed=1;
            if(batch_mask[i] && batch_generations[i*32 +:32]!=saved_generations[i*32 +:32]) bad_batch=1;
        end
    end
    function automatic [1:0] first_motor(input [3:0] mask);
        begin
            if(mask[0])first_motor=0;else if(mask[1])first_motor=1;
            else if(mask[2])first_motor=2;else first_motor=3;
        end
    endfunction
    wire [3:0] after_current=remaining & ~(4'b0001<<engine_motor);
    // HLS completion is registered and follows a previously accepted start.
    // Do not feed reset-qualified engine_ready into live fault detection:
    // live_fault -> engine_reset -> engine_ready would close a feedback loop.
    wire transaction_accepted=issued;
    wire accepting_engine=engine_start && engine_ready;
    wire bad_result=result_motor!=engine_motor || result_tick!=frame_tick ||
                    result_session!=saved_session || result_generation!=engine_generation;
    wire result_failure=(state==RUN && engine_done && transaction_accepted && (engine_fault || bad_result)) ||
                        (state==DRAIN && engine_done && engine_fault);
    wire protocol_failure=(state==ADMIT && admission_done && batch_valid && bad_batch) ||
                          (state==AGE_ACK && !target_accepted) ||
                          (state==RETIRE && retire_done && !retire_success);
    wire live_fault=busy && (deadline || overrun || tags_changed || result_failure || protocol_failure);
    wire cancellation=cancel_frame || (busy && enabled!=saved_enabled);
    assign busy=state!=IDLE;
    assign fault_inhibit=reset || inhibit || target_inhibit || fault || live_fault;
    assign engine_reset=fault_inhibit;
    // Requests must not depend on faults decoded from their own responses.
    // Such a dependency makes request -> done -> live_fault -> request cyclic.
    // Response failures still inhibit PWM immediately and win at the next edge.
    wire request_blocked=reset || inhibit || target_inhibit || fault ||
                         deadline || overrun || tags_changed || cancellation;
    assign admission_request=state==ADMIT && !request_blocked;
    assign engine_session=saved_session;
    assign engine_generation=saved_generations[engine_motor*32 +:32];
    assign engine_snapshot=saved_snapshots[engine_motor*SNAPSHOT_BITS +:SNAPSHOT_BITS];
    assign engine_target=saved_targets[engine_motor*32 +:32];
    assign engine_apply_target=has_batch && saved_mask[engine_motor];
    // Keep prospective target-age rejection out of its own valid qualification;
    // feeding target_inhibit back here would create a combinational loop.
    assign target_apply=state==AGE && !reset && !inhibit && !fault &&
                        !deadline && !overrun && !tags_changed && !cancellation;
    assign target_mask=saved_mask;
    assign retire_request=state==RETIRE && !request_blocked;
    assign retire_session=saved_session;
    assign retire_sequence=saved_sequence;
    // Present the whole staged frame BEFORE the common PWM sampling edge. All
    // dmotor_pwm channels must share this clock/reset and PWM_CYCLES value.
    assign pwm_duties=fault_inhibit ? 128'd0 :
        ((state==PWM_WAIT && pwm_boundary && !cancellation) ? staged_duties : committed_duties);
    always @(posedge clk) begin
        if(reset) begin
            state<=IDLE;timestamp<=0;frame_tick<=0;tick_count<=0;pwm_phase<=0;age<=0;
            saved_session<=0;saved_sequence<=0;saved_generations<=0;saved_targets<=0;
            saved_snapshots<=0;saved_enabled<=0;saved_mask<=0;remaining<=0;
            has_batch<=0;issued<=0;engine_start<=0;engine_motor<=0;fault<=0;
            staged_duties<=0;committed_duties<=0;frame_aborted<=0;frame_committed<=0;
            last_latency<=0;maximum_latency<=0;
        end else begin
            timestamp<=timestamp+1'b1;
            tick_count<=tick_due ? 0 : tick_count+1'b1;
            pwm_phase<=pwm_boundary ? 0 : pwm_phase+1'b1;
            frame_aborted<=0;frame_committed<=0;
            // Disabled channels must not replay an old duty when re-enabled.
            for(m=0;m<4;m=m+1) if(!enabled[m]) committed_duties[m*32 +:32]<=0;
            if(calculating) age<=age+1'b1;
            if(inhibit || target_inhibit || fault || live_fault) begin
                if(live_fault) fault<=1;
                if(busy) frame_aborted<=1;
                state<=IDLE;engine_start<=0;issued<=0;committed_duties<=0;staged_duties<=0;
            end else if(cancellation && busy && state!=DRAIN) begin
                frame_aborted<=1;engine_start<=0;staged_duties<=0;
                // Accepted HLS calls are drained, not reset, preserving peer contexts.
                if(state==RUN && (transaction_accepted || accepting_engine) && !engine_done) state<=DRAIN;
                else state<=IDLE;
            end else begin
                case(state)
                IDLE: if(tick_due && |enabled && !cancel_frame) begin
                    frame_tick<=timestamp;age<=0;saved_session<=session;saved_generations<=generations;
                    saved_snapshots<=snapshots;saved_enabled<=enabled;remaining<=enabled;
                    staged_duties<=0;has_batch<=0;saved_mask<=0;state<=ADMIT;
                end
                ADMIT: if(admission_done) begin
                    has_batch<=batch_valid;saved_mask<=batch_valid ? batch_mask : 0;
                    saved_sequence<=batch_sequence;saved_targets<=batch_targets;
                    engine_motor<=first_motor(remaining);engine_start<=1;issued<=0;state<=RUN;
                end
                RUN: begin
                    if(engine_start && engine_ready) begin engine_start<=0;issued<=1;end
                    if(engine_done && transaction_accepted) begin
                        engine_start<=0;issued<=0;
                        staged_duties[engine_motor*32 +:32]<=result_duty;
                        remaining<=after_current;
                        if(|after_current)begin engine_motor<=first_motor(after_current);engine_start<=1;end
                        else if(has_batch) state<=AGE;
                        else begin
                            state<=PWM_WAIT;last_latency<=age+1'b1;
                            if(age+1'b1>maximum_latency) maximum_latency<=age+1'b1;
                        end
                    end
                end
                AGE: state<=AGE_ACK;
                AGE_ACK: state<=RETIRE;
                RETIRE: if(retire_done) begin
                    state<=PWM_WAIT;last_latency<=age+1'b1;
                    if(age+1'b1>maximum_latency) maximum_latency<=age+1'b1;
                end
                PWM_WAIT: if(pwm_boundary)begin
                    committed_duties<=staged_duties;frame_committed<=1;state<=IDLE;
                end
                DRAIN: if(engine_done)begin state<=IDLE;issued<=0;end
                default: begin fault<=1;state<=IDLE;engine_start<=0;end
                endcase
            end
        end
    end
endmodule
