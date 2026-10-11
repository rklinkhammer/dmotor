// X1/X2/X4 Gray decoder. Configure mode/polarity and initialize only disabled.
// mode=0 X1 (QA rising), 1 X2 (QA edges), 2 X4. Position overflow saturates.
module qcencoder (
    input wire clk, reset, initialize,
    input wire qa, qb, invert,
    input wire [1:0] mode,
    input wire signed [63:0] initial_position,
    output reg signed [63:0] position,
    output reg [31:0] invalid_transitions,
    output reg range_fault, event_valid, event_negative,
    output reg [63:0] timestamp, event_timestamp
);
    (* ASYNC_REG = "TRUE" *) reg [1:0] meta, sync;
    reg [1:0] previous;
    reg [1:0] warmup;
    reg signed [2:0] delta;
    wire changed = sync != previous;
    wire accepted = mode == 2 || (mode == 1 && sync[1] != previous[1]) ||
                    (mode == 0 && sync[1] && !previous[1]);
    always @* begin
        case ({previous,sync})
            4'b0001,4'b0111,4'b1110,4'b1000: delta=1;
            4'b0010,4'b1011,4'b1101,4'b0100: delta=-1;
            default: delta=0;
        endcase
        if(invert) delta=-delta;
    end
    always @(posedge clk) begin
        if(reset) begin
            meta<=0; sync<=0; previous<=0; warmup<=0; position<=0;
            invalid_transitions<=0; range_fault<=0; event_valid<=0;
            event_negative<=0; timestamp<=0; event_timestamp<=0;
        end else begin
            meta<={qa,qb}; sync<=meta; previous<=sync;
            timestamp<=timestamp+1'b1; event_valid<=0;
            if(warmup != 3) warmup<=warmup+1'b1;
            if(initialize) begin
                position<=initial_position; range_fault<=0;
                invalid_transitions<=0; warmup<=0;
            end else if(warmup == 3 && changed) begin
                if(delta==0) begin
                    if(invalid_transitions != 32'hffffffff) invalid_transitions<=invalid_transitions+1'b1;
                end else if(accepted) begin
                    if((delta>0 && position==64'sh7fffffffffffffff) ||
                       (delta<0 && position==64'sh8000000000000000)) range_fault<=1;
                    else position<=position+delta;
                    event_valid<=1; event_negative<=delta<0; event_timestamp<=timestamp;
                end
            end
        end
    end
endmodule
