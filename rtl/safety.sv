// Synchronous commands are qualified by the HLS register/session adapter and
// revalidated against live safety state at transaction retirement.
// local_faults are already synchronized/qualified; halt_async is synchronized here.
module dmotor_safety (
    input wire clk, reset,
    input wire halt_async,
    input wire [31:0] local_faults,
    input wire enable_request, disable_request, prerequisites_valid,
    input wire acknowledge_local, acknowledge_external,
    input wire [63:0] timeout_cycles,
    input wire refresh_valid,
    input wire [31:0] session, refresh_session, refresh_sequence,
    output reg enabled,
    output wire inhibit, halt_out,
    output reg external_latched,
    output reg [31:0] faults,
    output reg [31:0] expected_sequence,
    output reg [63:0] timestamp, last_refresh, first_fault_timestamp,
    output wire halt_detected,
    input wire [31:0] acknowledge_mask
);
    (* ASYNC_REG = "TRUE" *) reg halt_meta, halt_sync;
    wire expired = enabled && (timestamp - last_refresh >= timeout_cycles);
    wire [31:0] new_faults = local_faults | (expired ? 32'h00000001 : 0);
    assign halt_detected = halt_sync;
    assign halt_out = |faults;
    assign inhibit = reset || !enabled || external_latched || halt_sync ||
                     (|faults) || (|new_faults);
    always @(posedge clk) begin
        if (reset) begin
            halt_meta <= 1'b0;
            halt_sync <= 1'b0;
            enabled <= 1'b0;
            external_latched <= 1'b0;
            faults <= 0;
            expected_sequence <= 1;
            timestamp <= 0;
            last_refresh <= 0;
            first_fault_timestamp <= 0;
        end else begin
            halt_meta <= halt_async;
            halt_sync <= halt_meta;
            timestamp <= timestamp + 1'b1;
            // New simultaneous causes win over acknowledgment.
            if (acknowledge_local) faults <= faults & ~(acknowledge_mask & ~local_faults);
            if (|new_faults) begin
                faults <= faults | new_faults;
                if (faults == 0) first_fault_timestamp <= timestamp;
            end
            if (acknowledge_external && !halt_sync) external_latched <= 1'b0;
            if (halt_sync) external_latched <= 1'b1;
            if (halt_sync || external_latched || (|faults) || (|new_faults) || disable_request) begin
                enabled <= 1'b0;
            end else if (!enabled) begin
                if (enable_request && prerequisites_valid && timeout_cycles != 0) begin
                    enabled <= 1'b1;
                    last_refresh <= timestamp;
                    expected_sequence <= 1;
                end
            end else if (refresh_valid && refresh_session == session &&
                         refresh_sequence == expected_sequence && expected_sequence != 0) begin
                last_refresh <= timestamp;
                // Exhausted sequence cannot wrap/reuse without a fresh session.
                expected_sequence <= expected_sequence + 1'b1;
            end
        end
    end
endmodule

module dmotor_halt_aggregator #(parameter MOTORS = 4) (
    input wire clk, reset, acknowledge,
    input wire [MOTORS-1:0] requests,
    output reg halted
);
    always @(posedge clk) begin
        if (reset) halted <= 1'b0; // Integration must hold every output inhibited.
        else if (|requests) halted <= 1'b1;
        else if (acknowledge) halted <= 1'b0;
    end
endmodule
