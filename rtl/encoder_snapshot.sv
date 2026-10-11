// Clock-domain event bookkeeping only; velocity arithmetic lives in C++ HLS.
// Feed qcencoder's registered event/diagnostic outputs. Reset this collector and
// the estimator whenever decoder position, polarity, mode or scale is initialized.
// The HLS adapter must latch ALL outputs and fabric time on the same clock edge.
module dmotor_encoder_snapshot (
    input wire clk, reset,
    input wire event_valid, event_negative,
    input wire [63:0] event_timestamp,
    input wire [31:0] encoder_invalid_transitions,
    input wire encoder_range_fault,
    output reg [63:0] count, last_edge,
    output reg [31:0] direction_epoch, invalid_transitions,
    output reg negative, capture_fault
);
    reg have_direction;
    always @(posedge clk) begin
        if (reset) begin
            count <= 0; last_edge <= 0; direction_epoch <= 0;
            invalid_transitions <= 0; negative <= 0;
            have_direction <= 0; capture_fault <= 0;
        end else begin
            invalid_transitions <= encoder_invalid_transitions;
            if (encoder_range_fault || encoder_invalid_transitions == 32'hffffffff)
                capture_fault <= 1;
            if (event_valid) begin
                count <= count + 1'b1;
                last_edge <= event_timestamp;
                if (have_direction && negative != event_negative)
                    direction_epoch <= direction_epoch + 1'b1;
                negative <= event_negative;
                have_direction <= 1;
            end
        end
    end
endmodule
