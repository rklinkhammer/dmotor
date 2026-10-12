// Independent of HLS and the authorization watchdog. applied_valid is a trusted,
// all-or-none successful batch-retirement pulse, never an MMIO commit/refresh.
module dmotor_target_age #(parameter MOTORS = 4) (
    input wire clk, reset,
    input wire [63:0] timestamp,
    input wire [MOTORS-1:0] enabled, stop_or_ready,
    input wire [MOTORS-1:0] configure, acknowledge,
    input wire [31:0] timeout_cycles,
    input wire applied_valid,
    input wire [MOTORS-1:0] applied_mask,
    input wire [63:0] applied_tick,
    output reg [MOTORS-1:0] configured, armed, faults,
    output reg [MOTORS-1:0] configure_accepted,
    output reg applied_accepted,
    output wire inhibit_all, halt_out,
    output wire [MOTORS-1:0] expired_now,
    output wire [MOTORS*64-1:0] age_cycles, last_applied_ticks, fault_timestamps
);
    import dmotor_timing::*;
    reg [31:0] limit [0:MOTORS-1];
    reg [63:0] last_tick [0:MOTORS-1];
    reg [63:0] fault_time [0:MOTORS-1];
    wire [63:0] incoming_age = timestamp - applied_tick;
    wire [MOTORS-1:0] incoming_expired;
    wire [MOTORS-1:0] eligible = enabled & configured & ~stop_or_ready;
    wire batch_eligible = applied_valid && (|applied_mask) &&
                         ((applied_mask & eligible) == applied_mask);
    // Future or ambiguous modulo timestamps are invalid, not fresh targets.
    wire batch_time_valid = !incoming_age[63];
    wire batch_accept = batch_eligible && batch_time_valid &&
                        !(|incoming_expired) && !(|expired_now) && !(|faults);
    genvar m;
    generate for (m=0;m<MOTORS;m=m+1) begin : channel
        wire [63:0] age = timestamp - last_tick[m];
        assign age_cycles[m*64 +: 64] = armed[m] ? age : 64'd0;
        assign last_applied_ticks[m*64 +: 64] = last_tick[m];
        assign fault_timestamps[m*64 +: 64] = fault_time[m];
        assign expired_now[m] = armed[m] && (age >= {32'd0,limit[m]});
        assign incoming_expired[m] = batch_eligible && applied_mask[m] &&
                                    (!batch_time_valid || incoming_age >= {32'd0,limit[m]});
        always @(posedge clk) begin
            if (reset) begin
                configured[m] <= 0; armed[m] <= 0; faults[m] <= 0;
                configure_accepted[m] <= 0; limit[m] <= 0;
                last_tick[m] <= 0; fault_time[m] <= 0;
            end else begin
                configure_accepted[m] <= 0;
                // Limits can change only while disabled and disarmed.
                if (configure[m] && !enabled[m] && !armed[m] &&
                    timeout_cycles >= TARGET_TIMEOUT_MIN_CYCLES &&
                    timeout_cycles <= TARGET_TIMEOUT_MAX_CYCLES) begin
                    limit[m] <= timeout_cycles;
                    configured[m] <= 1;
                    configure_accepted[m] <= 1;
                end
                // New expiration wins acknowledgment, stop, disable and activation.
                if (expired_now[m] || incoming_expired[m]) begin
                    faults[m] <= 1;
                    if (!faults[m]) fault_time[m] <= timestamp;
                end else if (acknowledge[m] && !enabled[m] && !armed[m]) begin
                    faults[m] <= 0;
                end
                if (inhibit_all || !enabled[m] || stop_or_ready[m]) armed[m] <= 0;
                else if (batch_accept && applied_mask[m]) begin
                    armed[m] <= 1;
                    last_tick[m] <= applied_tick;
                end
            end
        end
    end endgenerate
    assign halt_out = |faults;
    assign inhibit_all = reset || (|faults) || (|expired_now) || (|incoming_expired);
    always @(posedge clk) begin
        if (reset) applied_accepted <= 0;
        else applied_accepted <= batch_accept;
    end
endmodule
