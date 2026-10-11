// ap_ctrl_hs adapter: start remains asserted until ready. Inputs must be held
// from launch through completion. inhibit resets/drains the arithmetic adapter.
module dmotor_scheduler #(
    parameter integer TICK_CYCLES = 100000,
    parameter integer DEADLINE_CYCLES = 80000
) (
    input wire clk, reset, inhibit,
    input wire ready, done,
    output reg start, busy, result_valid, deadline_fault,
    output wire engine_reset,
    output reg [63:0] cycles,
    output reg [31:0] last_latency, maximum_latency
);
    reg [31:0] tick_count, age;
    assign engine_reset = reset || inhibit || deadline_fault;
    always @(posedge clk) begin
        if (reset) begin
            tick_count <= 0; age <= 0; start <= 0; busy <= 0;
            result_valid <= 0; deadline_fault <= 0;
            cycles <= 0; last_latency <= 0; maximum_latency <= 0;
        end else begin
            cycles <= cycles + 1'b1;
            result_valid <= 0;
            if (inhibit) begin
                tick_count <= 0; age <= 0; start <= 0; busy <= 0;
            end else if (!deadline_fault) begin
                tick_count <= tick_count == TICK_CYCLES-1 ? 0 : tick_count + 1'b1;
                if (start && ready) start <= 0;
                if (busy) begin
                    age <= age + 1'b1;
                    if (age >= DEADLINE_CYCLES-1 || tick_count == TICK_CYCLES-1) begin
                        deadline_fault <= 1; busy <= 0; start <= 0;
                    end else if (done) begin
                        result_valid <= 1; busy <= 0; start <= 0;
                        last_latency <= age + 1'b1;
                        if (age + 1'b1 > maximum_latency) maximum_latency <= age + 1'b1;
                    end
                end else if (tick_count == TICK_CYCLES-1) begin
                    start <= 1; busy <= 1; age <= 0;
                end
            end
        end
    end
endmodule
