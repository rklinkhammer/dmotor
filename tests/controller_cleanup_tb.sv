`timescale 1ns/1ps
module controller_cleanup_tb;
 reg clk=0;always #25 clk=~clk;
 reg reset=1;
 reg [15:0] awaddr=0,araddr=0;
 reg awvalid=0,wvalid=0,bready=0,arvalid=0,rready=0;
 reg [31:0] wdata=0;reg [3:0] wstrb=15;
 wire awready,wready,bvalid,arready,rvalid;
 wire [1:0] bresp,rresp;wire [31:0] rdata;
 reg [3:0] qa=0,qb=0,physical_faults=0;reg halt_async=0;
 reg hb_async=0,hb_run=1;
 always begin repeat(4)@(negedge clk);if(hb_run)hb_async=~hb_async;end
 wire [3:0] pwm,drive_enable,direction;wire shared_halt,coast_req;
 dmotor_controller_axi dut(.*);
 reg [3:0] sampled_drive_enable;
 always @(posedge clk) begin
  sampled_drive_enable=reset ? 4'b0 : dut.drive_enable_raw;
  #1;
  if(drive_enable!==sampled_drive_enable)
   $fatal(1,"GPIO diagnostic is not the registered raw drive state");
 end
 // Accelerate only the HB supervisor; retain real controller/HLS timing.
 defparam dut.monitor.STARTUP_CYCLES=80;
 defparam dut.monitor.TIMEOUT_CYCLES=40;
 defparam dut.monitor.QUALIFY_CYCLES=32;
 defparam dut.monitor.MIN_EDGE_CYCLES=2;
 task step;begin @(posedge clk);#1;end endtask
 task write_word(input [15:0] a,input [31:0] d,input [1:0] expected);begin
  @(negedge clk);awaddr=a;awvalid=1;wdata=d;wvalid=1;
  fork
   begin @(posedge clk);while(!awready)@(posedge clk);@(negedge clk);awvalid=0;end
   begin @(posedge clk);while(!wready)@(posedge clk);@(negedge clk);wvalid=0;end
  join
  wait(bvalid);#1;if(bresp!=expected)$fatal(1,"write %h response %d expected %d (HB=%b core_reset=%b shared_halt=%b fault=%b initialized=%b configured=%h stop=%h)",a,bresp,expected,dut.monitor_qualified,dut.core_reset,shared_halt,dut.fault,dut.protocol_initialized,dut.configured,dut.stop);
  repeat(2)step();@(negedge clk);bready=1;step();@(negedge clk);bready=0;
 end endtask
 reg [31:0] value;
 task read_word(input [15:0] a,input [1:0] expected=0);begin
  @(negedge clk);araddr=a;arvalid=1;@(posedge clk);while(!arready)@(posedge clk);
  @(negedge clk);arvalid=0;wait(rvalid);#1;
  if(rresp!=expected)$fatal(1,"read failed %h",a);value=rdata;
  repeat(2)step();if(rdata!=value)$fatal(1,"unstable read");
  @(negedge clk);rready=1;step();@(negedge clk);rready=0;
 end endtask

 always @(posedge clk)if(!reset && dut.cleanup_pending && (pwm || drive_enable || dut.enabled))
  $fatal(1,"outputs enabled during cleanup");
 integer i,reset_calls=0;
 always @(posedge clk)if(!reset && dut.controller.controller.engine.transport.command_valid &&
     dut.controller.controller.engine.transport.command_ready && dut.controller.controller.engine.record[32])
  reset_calls=reset_calls+1;
 initial begin
  repeat(4)step();@(negedge clk);reset=0;
  write_word('h1008,7,0);
  read_word('h1070);if(value!=32'h434c4e31)$fatal(1,"cleanup capability");
  write_word('h1074,1,2); // read-only
  // One populated channel; other contexts remain unconfigured.
  for(i=0;i<20;i=i+1)write_word('h2000+i*4,i==0 ? 1 : 0,0);
  write_word('h2080,1,0);
  force dut.controller.controller.engine.transport.command_ready=0;
  write_word('h1020,0,0);
  write_word('h100c,1,2); // configured, but cleanup has not completed
  write_word('h0,1,2); // atomic host mutation is excluded during cleanup
  release dut.controller.controller.engine.transport.command_ready;
  read_word('h107c);if(value!=1)$fatal(1,"request identity");
  // Completion requires both actual atomic initialization and all motor resets.
  wait(dut.cleanup_complete);repeat(3)step();
  read_word('h1074);if(value!=2)$fatal(1,"combined cleanup not complete");
  read_word('h1078);if(value!=7)$fatal(1,"wrong cleanup session");
  if(reset_calls!=4)$fatal(1,"not all contexts cleaned");
  if(!dut.protocol_initialized || dut.enabled || drive_enable)$fatal(1,"cleanup changed motion state");
  // Releasing twice cannot replay completion or allocate another identity.
  write_word('h1020,0,2);
  write_word('h1020,1,0);
  read_word('h1074);if(value!=0)$fatal(1,"reset kept completion");
  write_word('h1008,8,0);write_word('h1020,0,0);
  wait(dut.controller.controller.engine.state==2);
  // Interrupt an accepted context reset in the actual HLS transport. An old
  // completion cannot satisfy the next request after the reset boundary.
  write_word('h1020,1,0);repeat(20)step();
  read_word('h1074);if(value!=0)$fatal(1,"late completion survived reset");
  write_word('h1008,9,0);write_word('h1020,0,0);
  wait(dut.cleanup_complete);
  read_word('h107c);if(value!=3)$fatal(1,"second request identity");
  read_word('h1078);if(value!=9)$fatal(1,"stale session");
  @(negedge clk);hb_run=0;
  repeat(70)step();read_word('h1074);
  if(value!=4 || !coast_req)$fatal(1,"HB fault retained cleanup proof");
  write_word('h1020,1,0);
  force dut.cleanup_request=32'hffffffff;
  write_word('h1020,0,2);release dut.cleanup_request;
  $display("controller_cleanup PASS: combined cleanup; completion, session, request, reset, fault, exhaustion");
  $finish;
 end
 initial begin repeat(100000)step();$fatal(1,"cleanup integration timeout state=%d inhibit=%b proto=%b fault=%b",dut.controller.controller.cleanup.state,dut.controller.global_inhibit,dut.controller.controller.protocol_quiescent,dut.fault);end
endmodule
