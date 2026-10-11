// Logical motor output only, not the unvalidated DRV8263H electrical adapter.
// duty magnitude Q16; config stable while enabled. Reversal waits one full
// PWM frame with drive disabled, then latches direction on a later boundary.
module dmotor_pwm # (parameter integer PERIOD_CYCLES=dmotor_timing::PWM_PERIOD_CYCLES) (
    input wire clk, reset, inhibit,
    input wire signed [31:0] requested_duty_q16,
    output wire pwm, drive_enable,
    output reg direction, reversing,
    output reg signed [31:0] applied_duty_q16,
    output reg [63:0] timestamp, application_timestamp
);
    reg [31:0] phase, magnitude;
    wire negative = requested_duty_q16 < 0;
    wire signed [63:0] wide_duty = requested_duty_q16;
    wire [63:0] absolute_duty = negative ? -wide_duty : wide_duty;
    wire [31:0] bounded = absolute_duty > 65536 ? 65536 : absolute_duty[31:0];
    wire [63:0] scaled = {32'd0,magnitude} * PERIOD_CYCLES;
    assign drive_enable = !reset && !inhibit && !reversing && magnitude != 0;
    assign pwm = drive_enable && phase < (scaled >> 16);
    always @(posedge clk) begin
        if(reset) begin
            phase<=0; magnitude<=0; direction<=0; reversing<=0;
            applied_duty_q16<=0; timestamp<=0; application_timestamp<=0;
        end else begin
            timestamp<=timestamp+1'b1;
            phase<=phase==PERIOD_CYCLES-1 ? 0 : phase+1'b1;
            if(inhibit) begin
                magnitude<=0; applied_duty_q16<=0; reversing<=0;
            end else if(phase==PERIOD_CYCLES-1) begin
                application_timestamp<=timestamp;
                if(bounded!=0 && negative!=direction && !reversing) begin
                    reversing<=1; magnitude<=0; applied_duty_q16<=0;
                end else begin
                    reversing<=0;
                    if(bounded!=0) direction<=negative;
                    magnitude<=bounded;
                    applied_duty_q16<=negative ? -$signed(bounded) : $signed(bounded);
                end
            end
        end
    end
endmodule
