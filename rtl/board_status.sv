// Board maintenance interlock and indicators. Independent of Linux/core_reset.
module dmotor_board_status #(
 parameter integer RELEASE_CYCLES=400000, // 20 ms at 20 MHz
 parameter integer BLINK_HALF_CYCLES=5000000 // 2 Hz full waveform
)(
 input wire clk,reset,maintenance_async,acknowledge,drive_authorized,hb_healthy,
 output wire inhibited,
 output wire inhibit_red,inhibit_green,inhibit_blue,hb_red,hb_green,hb_blue
);
 (* ASYNC_REG="TRUE" *) reg sw_meta,sw_sync;
 reg latched;
 localparam integer RB=RELEASE_CYCLES>1?$clog2(RELEASE_CYCLES+1):1;
 localparam integer BB=BLINK_HALF_CYCLES>1?$clog2(BLINK_HALF_CYCLES):1;
 reg [RB-1:0] released;
 reg [BB-1:0] blink_count;
 reg blink;
 assign inhibited=reset || sw_sync || latched;
 assign inhibit_red=blink && (inhibited || !drive_authorized);
 assign inhibit_blue=blink && !inhibited && drive_authorized;
 assign inhibit_green=1'b0;
 assign hb_blue=blink && hb_healthy && !reset;
 assign hb_red=blink && !hb_healthy && !reset;
 assign hb_green=1'b0;
 always @(posedge clk)begin
  if(reset)begin
   sw_meta<=1;sw_sync<=1;latched<=1;released<=0;blink_count<=0;blink<=0;
  end else begin
   sw_meta<=maintenance_async;sw_sync<=sw_meta;
   if(sw_sync)begin released<=0;latched<=1;end
   else begin
    if(released<RELEASE_CYCLES)released<=released+1'b1;
    if(acknowledge && released>=RELEASE_CYCLES)latched<=0;
   end
   if(blink_count==BLINK_HALF_CYCLES-1)begin blink_count<=0;blink<=!blink;end
   else blink_count<=blink_count+1'b1;
  end
 end
endmodule
