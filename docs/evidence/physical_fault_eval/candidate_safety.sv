// Synchronous commands are qualified by the HLS register/session adapter and
// revalidated against live safety state at transaction retirement.
// local_faults are already synchronized/qualified; halt_async is synchronized here.
module candidate_safety #(parameter HALT_ALREADY_SYNCHRONIZED = 0) (
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
    output wire [31:0] faults,
    input wire physical_latched,
    output wire any_new_fault,
    output reg [31:0] expected_sequence,
    output reg [63:0] timestamp, last_refresh, first_fault_timestamp,
    output wire halt_detected,
    input wire [31:0] acknowledge_mask
);
    reg [31:0] local_latched;
    assign faults = local_latched | (physical_latched ? 32'h4 : 0);
    assign any_new_fault = |new_faults;
    wire halt_sync;
    generate if (HALT_ALREADY_SYNCHRONIZED) begin: shared_halt_input
        // Only for a signal already synchronized to this clk at the shared boundary.
        assign halt_sync = halt_async;
    end else begin: local_halt_input
        (* ASYNC_REG = "TRUE" *) reg halt_meta, halt_stage;
        assign halt_sync = halt_stage;
        always @(posedge clk) begin
            if(reset) begin halt_meta<=0;halt_stage<=0;end
            else begin halt_meta<=halt_async;halt_stage<=halt_meta;end
        end
    end endgenerate
    wire expired = enabled && (timestamp - last_refresh >= timeout_cycles);
    wire [31:0] new_faults = local_faults | (expired ? 32'h00000001 : 0);
    assign halt_detected = halt_sync;
    assign halt_out = |faults;
    assign inhibit = reset || !enabled || external_latched || halt_sync ||
                     (|faults) || (|new_faults);
    always @(posedge clk) begin
        if (reset) begin
            enabled <= 1'b0;
            external_latched <= 1'b0;
            local_latched <= 0;
            expected_sequence <= 1;
            timestamp <= 0;
            last_refresh <= 0;
            first_fault_timestamp <= 0;
        end else begin
            timestamp <= timestamp + 1'b1;
            // New simultaneous causes win over acknowledgment.
            if (acknowledge_local) local_latched <= faults & ~(acknowledge_mask & ~local_faults) & ~32'h4;
            if (|new_faults) begin
                local_latched <= (faults | new_faults) & ~32'h4;
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
