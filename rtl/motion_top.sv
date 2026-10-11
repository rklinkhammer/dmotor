// Thin Vitis adapter: policy and arithmetic live in dmotor_motion C++.
// All inputs are in clk domain. Config is stable for the enabled session.
// Mailbox commands/ack/stop must be held until inputs_sampled; command data
// remains pending until the accepted flag in a retired result. Protection must
// separately cancel the ring; reset/inhibit discards the in-flight HLS state.
module dmotor_motion_top (
    input wire clk, reset, inhibit,
    input wire [511:0] command_words, configuration_words,
    input wire command_valid, completion_ack, immediate_stop,
    input wire [31:0] session,
    input wire [63:0] timestamp,
    input wire signed [63:0] position,
    input wire signed [31:0] velocity_mrpm,
    input wire velocity_valid,
    output reg inputs_sampled, result_valid,
    output reg [511:0] result_words,
    output wire pwm, drive_enable, direction,
    output wire deadline_fault,
    output reg [31:0] motion_faults,
    output wire [31:0] last_latency, maximum_latency
);
    wire start, done, ready, retired, engine_reset;
    wire protected_state = inhibit || motion_faults != 0;
    reg start_seen;
    reg [511:0] command_snapshot, config_snapshot;
    reg valid_snapshot, ack_snapshot, stop_snapshot, feedback_snapshot;
    reg [31:0] session_snapshot;
    reg [63:0] time_snapshot, position_snapshot;
    reg [31:0] velocity_snapshot;
    reg signed [31:0] duty;
    reg [31:0] reply [0:15];
    wire [3:0] ca, fa0, fa1, ra0, ra1;
    wire cc, fc0, fc1, rc0, rc1, rw0, rw1;
    reg [31:0] cq, fq0, fq1;
    wire [31:0] rd0, rd1;
    integer i;
    dmotor_scheduler scheduler (
        .clk(clk), .reset(reset), .inhibit(protected_state),
        .ready(ready), .done(done), .start(start), .busy(),
        .result_valid(retired), .deadline_fault(deadline_fault),
        .engine_reset(engine_reset), .cycles(),
        .last_latency(last_latency), .maximum_latency(maximum_latency)
    );
    dmotor_motion kernel (
        .ap_clk(clk), .ap_rst(engine_reset), .ap_start(start && start_seen),
        .ap_done(done), .ap_idle(), .ap_ready(ready), .reset(1'b0),
        .authorized(1'b1), .command_valid(valid_snapshot),
        .command_address0(ca), .command_ce0(cc), .command_q0(cq),
        .configuration_r_address0(fa0), .configuration_r_ce0(fc0), .configuration_r_q0(fq0),
        .configuration_r_address1(fa1), .configuration_r_ce1(fc1), .configuration_r_q1(fq1),
        .completion_ack(ack_snapshot), .immediate_stop(stop_snapshot),
        .session(session_snapshot), .now(time_snapshot), .position(position_snapshot),
        .velocity_mrpm(velocity_snapshot), .velocity_valid(feedback_snapshot),
        .reply_address0(ra0), .reply_ce0(rc0), .reply_we0(rw0), .reply_d0(rd0),
        .reply_address1(ra1), .reply_ce1(rc1), .reply_we1(rw1), .reply_d1(rd1)
    );
    // ap_memory input ports have one-cycle synchronous read latency.
    always @(posedge clk) begin
        if (cc) cq <= command_snapshot[ca*32 +: 32];
        if (fc0) fq0 <= config_snapshot[fa0*32 +: 32];
        if (fc1) fq1 <= config_snapshot[fa1*32 +: 32];
        if (rc0 && rw0) reply[ra0] <= rd0;
        if (rc1 && rw1) reply[ra1] <= rd1;
    end
    always @(posedge clk) begin
        inputs_sampled <= 0;
        result_valid <= 0;
        if (reset) begin
            start_seen <= 0; duty <= 0; motion_faults <= 0; result_words <= 0;
            command_snapshot <= 0; config_snapshot <= 0;
            valid_snapshot <= 0; ack_snapshot <= 0; stop_snapshot <= 0;
            session_snapshot <= 0; time_snapshot <= 0; position_snapshot <= 0;
            velocity_snapshot <= 0; feedback_snapshot <= 0;
        end else if (engine_reset) begin
            start_seen <= 0; duty <= 0; result_words <= 0;
        end else begin
            if (!start) start_seen <= 0;
            if (start && !start_seen) begin
                start_seen <= 1;
                inputs_sampled <= 1;
                command_snapshot <= command_words; config_snapshot <= configuration_words;
                valid_snapshot <= command_valid; ack_snapshot <= completion_ack;
                stop_snapshot <= immediate_stop; session_snapshot <= session;
                time_snapshot <= timestamp; position_snapshot <= position;
                velocity_snapshot <= velocity_mrpm; feedback_snapshot <= velocity_valid;
            end
            if (retired) begin
                result_valid <= 1;
                for (i = 0; i < 16; i = i + 1) result_words[i*32 +: 32] <= reply[i];
                motion_faults <= motion_faults | reply[7];
                duty <= reply[7] == 0 ? $signed(reply[0]) : 0;
            end
        end
    end
    dmotor_pwm output_stage (
        .clk(clk), .reset(reset), .inhibit(protected_state || deadline_fault),
        .requested_duty_q16(duty), .pwm(pwm), .drive_enable(drive_enable),
        .direction(direction), .reversing(), .applied_duty_q16(),
        .timestamp(), .application_timestamp()
    );
endmodule
