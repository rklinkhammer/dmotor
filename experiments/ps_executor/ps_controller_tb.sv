module ps_controller_tb;
 reg clk=0;always #5 clk=~clk;
 reg reset=1;reg [15:0] awaddr=0,araddr=0;reg awvalid=0,wvalid=0,arvalid=0;
 reg [31:0] wdata=0;reg [3:0] wstrb=15;wire awready,wready,arready;
 wire [1:0] bresp,rresp;wire bvalid,rvalid;reg bready=0,rready=0;wire [31:0] rdata;
 reg [3:0] qa=0,qb=0,physical_faults=0;reg halt_async=0,hb_async=0,hb_run=1;
 wire [3:0] pwm,direction,drive_enable;wire shared_halt,coast_req;
 integer hb_count=0;
 reg rotate=0;integer rotation_count=0;reg [1:0] rotation_state=0;
 always @(negedge clk)begin
  if(reset)begin rotation_count=0;rotation_state=0;end
  else if(rotate)begin
   rotation_count=rotation_count+1;
   if(rotation_count==3)begin
    rotation_count=0;
    case(rotation_state)
     0:begin qa=0;qb=15;end
     1:begin qa=15;qb=15;end
     2:begin qa=15;qb=0;end
     3:begin qa=0;qb=0;end
    endcase
    rotation_state=rotation_state+1'b1;
   end
  end
 end
 always @(negedge clk)begin
  if(reset)begin hb_count=0;hb_async=0;end
  else begin
   hb_count=hb_count+1;if(hb_count==10)begin hb_count=0;if(hb_run)hb_async=~hb_async;end
  end
 end
 ps_controller_axi #(.PWM_CYCLES(16),.CAPTURE_MAX(3000),.DUTY_TIMEOUT(5000),
  .AUTH_TIMEOUT(9000),.HB_STARTUP(500),.HB_TIMEOUT(100),.HB_QUALIFY(20),.HB_MIN(2)) dut(.*);
 task wr(input [15:0] a,input [31:0] d,input bit okay);
 begin
  @(negedge clk);awaddr=a;wdata=d;awvalid=1;wvalid=1;
  // This endpoint independently buffers both channels; both are initially empty.
  do @(posedge clk);while(!(awready&&wready));
  @(negedge clk);awvalid=0;wvalid=0;bready=1;
  do @(posedge clk);while(!bvalid);
  if((bresp==0)!=okay)$fatal(1,"write %h response %d expected okay=%d",a,bresp,okay);
  @(negedge clk);bready=0;
 end endtask
 task rd(input [15:0] a,output [31:0] d);
 begin
  @(negedge clk);araddr=a;arvalid=1;
  do @(posedge clk);while(!arready);
  @(negedge clk);arvalid=0;rready=1;
  do @(posedge clk);while(!rvalid);
  if(rresp!=0)$fatal(1,"read failed");d=rdata;
  @(negedge clk);rready=0;
 end endtask
 task cold;
 begin
  @(negedge clk);reset=1;rotate=0;qa=0;qb=0;physical_faults=0;halt_async=0;hb_run=1;
  repeat(8)@(posedge clk);@(negedge clk);reset=0;
  repeat(80)@(posedge clk);
 end endtask
 reg [31:0] tok,val,count_val;
 task capture;
 begin wr(16,1,1);rd(16,tok);end endtask
 task stage(input [31:0] sess,seq,token,input [31:0] d0);
 begin
  wr(64,sess,1);wr(68,seq,1);wr(72,token,1);wr(76,d0,1);
  wr(80,32768,1);wr(84,-32768,1);wr(88,-65536,1);
 end endtask
 task frame(input [31:0] seq);
 begin capture;stage(1,seq,tok,65536);wr(92,1,1);repeat(40)@(posedge clk);end endtask
 integer n;
 initial begin
  cold;rd(0,val);if(val!=32'h50534531)$fatal(1,"identity");
  rotate=1;
  for(n=0;n<16;n=n+1)begin
   capture;rd(128,val);rd(136,count_val);
   if(val!=count_val)$fatal(1,"snapshot position/count pipeline mismatch");
  end
  cold;
  wr(8,1,1);capture;
  wr(64,1,1);wr(92,1,0); // partial frame
  stage(2,1,tok,65536);wr(92,1,0); // wrong session
  stage(1,1,tok,65537);wr(92,1,0); // out of range
  stage(1,1,tok,65536);wr(92,1,1);
  repeat(50)@(posedge clk);
  rd(32,val);if(val!=1)$fatal(1,"apply sequence");
  rd(256,val);if($signed(val)!=65536)$fatal(1,"wheel0");
  rd(260,val);if($signed(val)!=32768)$fatal(1,"wheel1");
  rd(264,val);if($signed(val)!=-32768)$fatal(1,"wheel2");
  rd(268,val);if($signed(val)!=-65536)$fatal(1,"wheel3");
  stage(1,2,tok,65536);wr(92,1,0); // same snapshot cannot refresh output age
  capture;stage(1,1,tok,65536);wr(92,1,0); // replay
  capture;stage(1,2,tok,65536);
  while(dut.now-dut.snapshot_tick<dut.REQUEST_MAX)@(posedge clk);
  wr(92,1,0); // reserve reversal time before the full capture deadline
  repeat(2200)@(posedge clk);if(!shared_halt||pwm!=0)$fatal(1,"stale duty must inhibit");
  wr(8,2,0); // cannot restart after fault
  cold;wr(8,1,1);frame(1);halt_async=1;
  repeat(10)@(posedge clk);if(!coast_req||pwm!=0)$fatal(1,"HALT");
  halt_async=0;repeat(10)@(posedge clk);if(!shared_halt)$fatal(1,"HALT sticky");
  cold;wr(8,1,1);frame(1);physical_faults=1;
  repeat(10)@(posedge clk);if(!shared_halt||pwm!=0)$fatal(1,"physical fault");
  cold;wr(8,1,1);frame(1);hb_run=0;
  repeat(150)@(posedge clk);if(!shared_halt||pwm!=0)$fatal(1,"HB loss");
  cold;wr(8,1,1);frame(1);qa=1;qb=1;
  repeat(10)@(posedge clk);if(!shared_halt)$fatal(1,"encoder diagnostic");
  cold;wr(8,1,1);frame(1);wr(96,1,1);
  repeat(3)@(posedge clk);if(pwm!=0||dut.armed)$fatal(1,"disable");
  wr(8,1,0);wr(8,2,1); // explicit fresh session, no fault
  cold;wr(8,1,1);
  for(n=1;n<=10;n=n+1)begin frame(n);repeat(650)@(posedge clk);end
  repeat(2200)@(posedge clk);if(!shared_halt)$fatal(1,"authorization independent of duty");
  cold;wr(8,1,1);frame(1);
  // Block AXI response consumption: protection must still progress.
  @(negedge clk);awaddr=16;wdata=1;awvalid=1;wvalid=1;bready=0;
  do @(posedge clk);while(!(awready&&wready));
  @(negedge clk);awvalid=0;wvalid=0;
  repeat(5100)@(posedge clk);
  if(!shared_halt||pwm!=0)$fatal(1,"AXI backpressure blocked protection");
  @(negedge clk);bready=1;repeat(3)@(posedge clk);@(negedge clk);bready=0;
  $display("PASS: AXI four-wheel atomic duties, range/session/sequence/token/age checks, independent auth expiry, disable, HALT/HB/physical/encoder faults");
  $finish;
 end
 // Every accepted request vector becomes visible on the same common boundary.
 reg [31:0] old_applied=0;reg [127:0] old_committed=0;
 always @(negedge clk)begin
  if(!reset&&dut.committed_duty!=old_committed&&dut.committed_duty!=0)
   if(dut.phase!=0)$fatal(1,"non-boundary update");
  old_applied=dut.applied_sequence;old_committed=dut.committed_duty;
 end
 initial begin #3000000;$fatal(1,"test timeout");end
endmodule
