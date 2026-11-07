// Independent monitor-liveness supervisor used by controller_axi.
// All parameters are clock cycles; defaults assume the qualified 20 MHz clock.
module dmotor_monitor_heartbeat #(
 parameter integer STARTUP_CYCLES=20000000,
 parameter integer TIMEOUT_CYCLES=1000000,
 parameter integer QUALIFY_CYCLES=10000000,
 parameter integer MIN_EDGE_CYCLES=20000
)(
 input wire clk,reset,core_reset,hb_async,acknowledge,
 output reg fault,
 output wire qualified,
 output reg healthy
);
 (* ASYNC_REG="TRUE" *) reg hb_meta,hb_sync;
 reg previous,seen;
 localparam integer AGE_LIMIT = STARTUP_CYCLES > TIMEOUT_CYCLES ? STARTUP_CYCLES : TIMEOUT_CYCLES;
 localparam integer AGE_BITS = AGE_LIMIT > 1 ? $clog2(AGE_LIMIT+64'd1) : 1;
 localparam integer WINDOW_BITS = QUALIFY_CYCLES > 1 ? $clog2(QUALIFY_CYCLES+64'd1) : 1;
 reg [AGE_BITS-1:0] age;
 reg [WINDOW_BITS-1:0] window_age;
 wire edge_seen=hb_sync!=previous;
 wire expired=seen ? age>=TIMEOUT_CYCLES-1 : age>=STARTUP_CYCLES-1;
 wire too_fast=seen && edge_seen && age<MIN_EDGE_CYCLES-1;
 // Monitoring continues during CORE_RESET. Qualification is never motion
 // authorization by itself, and clearing core reset cannot clear a fault.
 assign qualified=healthy && !fault && !core_reset && !reset;
 always @(posedge clk) begin
  if(reset) begin
   hb_meta<=0;hb_sync<=0;previous<=0;seen<=0;
   age<=0;window_age<=0;healthy<=0;fault<=0;
  end else begin
   hb_meta<=hb_async;hb_sync<=hb_meta;previous<=hb_sync;
   if(expired || too_fast) begin
    fault<=1;healthy<=0;window_age<=0;
    // Restart observation after a loss; keep the fault sticky. A static
    // input cannot qualify, even when acknowledgment is held high.
    seen<=edge_seen;age<=0;
   end else begin
    if(edge_seen) begin seen<=1;age<=0;end
    else age<=age+1'b1;
    if(seen && !healthy) begin
     if(window_age>=QUALIFY_CYCLES-1)healthy<=1;
     else window_age<=window_age+1'b1;
    end
    if(acknowledge && healthy)fault<=0;
   end
  end
 end
endmodule
