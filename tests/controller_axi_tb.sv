`timescale 1ns/1ps
module controller_axi_tb;
 reg clk=0;always #25 clk=~clk;
 reg reset=1;
 reg [15:0] awaddr=0,araddr=0;
 reg awvalid=0,wvalid=0,bready=0,arvalid=0,rready=0;
 reg [31:0] wdata=0;reg [3:0] wstrb=15;
 wire awready,wready,bvalid,arready,rvalid;
 wire [1:0] bresp,rresp;wire [31:0] rdata;
 reg [3:0] qa=0,qb=0,physical_faults=0;reg halt_async=0;
 reg maintenance_async=0;
 wire led4_r,led4_g,led4_b,led5_r,led5_g,led5_b;
 reg hb_async=0,hb_run=1;
 always begin repeat(4)@(negedge clk);if(hb_run)hb_async=~hb_async;end
 wire [3:0] pwm,drive_enable,direction;wire shared_halt,coast_req;
 dmotor_controller_axi dut(.*);
 defparam dut.board_status.RELEASE_CYCLES=4;
 defparam dut.board_status.BLINK_HALF_CYCLES=8;
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
 reg [31:0] cfg[0:19];integer i,m;
 reg [2687:0] snapshot_pattern;
 reg seen_fault=0;
 always @(negedge clk)begin
  if(dut.enable_request) $display("enable request %h cfg=%h initialized=%b inhibit=%b local=%h ext=%h halt=%h enabled=%h time=%t",dut.enable_request,dut.configured,dut.protocol_initialized,dut.controller.global_inhibit,dut.watchdog_faults,dut.controller.external_latches,dut.controller.halt_detected,dut.enabled,$time);
  if(reset)seen_fault=0;
  else if(dut.fault && !seen_fault)begin
   seen_fault=1;
   $display("fault time=%t hls_fault=%h reject=%h core_fault=%b scheduler=%b stop=%h",$time,
    dut.controller.controller.engine.reply[256 +:32],dut.controller.controller.engine.reply[288 +:32],
    dut.controller.controller.engine.fault,dut.controller.controller.control.control.scheduler.fault,dut.stop);
  end
 end
 task direct_start(input [31:0] wd,input [31:0] age,input [31:0] limit=65536);begin
  @(negedge clk);reset=1;physical_faults=0;halt_async=0;repeat(4)step();@(negedge clk);reset=0;
  write_word('h1008,1,0);repeat(8)step();write_word('h1024,2,0);
  cfg[0]=1;for(i=0;i<20;i=i+1)write_word('h2000+i*4,cfg[i],0);
  write_word('h2080,1,0);
  write_word('h1054,3,2);write_word('h1054,1,0);write_word('h1058,limit,0);
  write_word('h1028,wd,0);write_word('h102c,age,0);
  // Reset may sample HB high just before its next edge. Recover explicitly
  // after qualification rather than depending on the stimulus phase.
  wait(dut.monitor_healthy);write_word('h106c,1,0);
  write_word('h1020,0,0);
  repeat(10)step();write_word('h100c,2,2);write_word('h100c,1,0);
  write_word('h1018,1,0);write_word('h1040,1,0);
 end endtask
 integer highs;
 initial begin
  for(i=0;i<20;i=i+1)cfg[i]=0;
  cfg[1]=200000;cfg[2]=1000000;cfg[3]=1000000;cfg[4]=10296;cfg[5]=5;
  cfg[6]=1;cfg[7]=100;cfg[8]=2;cfg[9]=655;cfg[10]=65536;cfg[11]=65536;cfg[12]=65536;
  cfg[13]=1000000;cfg[14]=40000;cfg[15]=20000;cfg[16]=128;
  repeat(4)step();@(negedge clk);reset=0;
  read_word('h1000);if(value!=32'h444d4932)$fatal(1,"identity");
  write_word('h1008,7,0);repeat(8)step();write_word('h1024,2,0);
  // Encoder-only utility lifecycle: no configuration, enable or watchdog writes.
  write_word('h1020,1,0);write_word('h1020,0,0);repeat(8)step();
  @(negedge clk);qb=1;repeat(8)step();@(negedge clk);qa=1;repeat(8)step();
  @(negedge clk);qb=0;repeat(8)step();@(negedge clk);qa=0;repeat(8)step();
  write_word('h1030,1,0);read_word('h3040);if(value!=4)$fatal(1,"disabled encoder count");
  if(drive_enable || dut.enabled || dut.configured)$fatal(1,"encoder utility enabled motor");
  @(negedge clk);qa=1;repeat(8)step();@(negedge clk);qb=1;repeat(8)step();
  @(negedge clk);qa=0;repeat(8)step();@(negedge clk);qb=0;repeat(8)step();
  write_word('h1030,1,0);read_word('h3040);if(value!=0)$fatal(1,"disabled reverse encoder count");
  write_word('h1020,1,0);repeat(8)step();
  write_word('h1030,1,0);read_word('h3040);if(value!=0)$fatal(1,"encoder cleanup reset");

  // Exercise every bank/word with distinct data before loading valid motor configs.
  for(m=0;m<4;m=m+1)begin
   write_word('h2080+m*256,1,2); // incomplete bank cannot commit
   for(i=0;i<20;i=i+1)write_word('h2000+m*256+i*4,32'ha5000000+m*256+i,0);
   for(i=0;i<20;i=i+1)begin
    read_word('h2000+m*256+i*4);if(value!=0)$fatal(1,"staging leaked before commit");
   end
   write_word('h2080+m*256,1,0);
  end
  for(m=0;m<4;m=m+1)for(i=0;i<20;i=i+1)begin
   read_word('h2000+m*256+i*4);
   if(value!=(32'ha5000000+m*256+i))$fatal(1,"configuration bank alias");
  end
  write_word('h2004,123,0);write_word('h2080,1,2); // partial replacement rejected
  read_word('h2004);if(value!=32'ha5000001)$fatal(1,"partial replacement escaped");
  read_word('h2050,2);read_word('h2400,2);read_word('h2001,2);
  wstrb=1;write_word('h2004,0,2);wstrb=15;
  // Distinct words expose telemetry selection aliases and boundary mistakes.
  for(i=0;i<84;i=i+1)snapshot_pattern[i*32 +:32]=32'h5a000000+i;
  force dut.snapshot_bank=snapshot_pattern;
  for(i=0;i<84;i=i+1)begin
   read_word('h3000+i*4);if(value!=(32'h5a000000+i))$fatal(1,"snapshot word alias");
  end
  read_word('h3150,2);read_word('h3001,2);write_word('h3000,0,2);
  release dut.snapshot_bank;
  for(m=0;m<4;m=m+1)begin
   write_word('h1040+m*4,32'h12340000+m,0);
   if(dut.refresh_sequences[m*32 +:32]!=(32'h12340000+m))$fatal(1,"refresh bank alias");
  end

  for(m=0;m<4;m=m+1)begin
   cfg[0]=21+m;
   for(i=0;i<20;i=i+1)write_word('h2000+m*256+i*4,cfg[i],0);
   write_word('h2080+m*256,1,0);
  end
  write_word('h1020,0,0);wait(dut.cleanup_complete || dut.direct_mode!=0);repeat(3)step();write_word('h100c,15,0);
  // A host cannot reconfigure an enabled context.
  write_word('h2004,100000,2);
  wait(dut.ready_mask==15);
  write_word('h10,7,0);write_word('h14,1,0);write_word('h18,15,0);
  for(m=0;m<4;m=m+1)begin write_word('h80+m*8,21+m,0);write_word('h84+m*8,(m+1)*1000,0);end
  write_word('h1c,1,0);
  value=0;while(value!=1)read_word('h2c);
  repeat(1200)step();if(dut.fault || drive_enable!=15)$fatal(1,"four motor application");
  write_word('h1030,1,0);read_word('h3008);if(value!=1)$fatal(1,"telemetry motor0 sequence");
  // STOP accepted during computation takes effect at the next complete control
  // boundary, without dropping a motor tick or publishing another target.
  wait(dut.controller.controller.engine_start);
  write_word('h1014,15,0);
  value=0;while((value & 32'hf000)!=32'hf000)read_word('h1004);
  repeat(1200)step();if(drive_enable || dut.fault || coast_req)$fatal(1,"STOP lifecycle failed or requested global coast");
  // Decoder capture is real: one Gray cycle, with independently synchronized pins.
  @(negedge clk);qb=1;repeat(8)step();@(negedge clk);qa=1;repeat(8)step();
  @(negedge clk);qb=0;repeat(8)step();@(negedge clk);qa=0;repeat(8)step();
  write_word('h1030,1,0);read_word('h3040);if(value!=4)$fatal(1,"encoder position %d",value);
  // Qualify a physical fault: all outputs inhibit, cause persists.
  @(negedge clk);physical_faults=4;repeat(8)step();
  if(!shared_halt || !coast_req || drive_enable)$fatal(1,"physical fault not shared");
  @(negedge clk);physical_faults=0;repeat(8)step();if(!shared_halt)$fatal(1,"automatic recovery");
  read_word('h1004);if(!value[17])$fatal(1,"HALT status");
  // Reset only controller arithmetic/session while retaining the protective latch.
  write_word('h1020,1,0);repeat(4)step();if(!shared_halt)$fatal(1,"core reset cleared protection");
  // Explicit recovery order, then watchdog expiration with multiple motors enabled.
  write_word('h1024,1,0);write_word('h1024,2,0);write_word('h1024,4,0);
  if(shared_halt)$fatal(1,"acknowledgments did not clear inactive causes");
  write_word('h1028,20000,0);write_word('h1020,0,0);wait(dut.cleanup_complete || dut.direct_mode!=0);repeat(3)step();
  write_word('h100c,3,0);wait(shared_halt);repeat(4)step();
  if(drive_enable || (dut.watchdog_faults[0]!==1'b1 && dut.watchdog_faults[32]!==1'b1))
   $fatal(1,"watchdog did not halt all motors");
  // External HALT blocks atomic traffic with SLVERR rather than hanging AXI.
  @(negedge clk);reset=1;repeat(3)step();@(negedge clk);reset=0;
  write_word('h1008,8,0);repeat(8)step();write_word('h1024,2,0);write_word('h1020,0,0);wait(dut.cleanup_complete || dut.direct_mode!=0);repeat(3)step();
  @(negedge clk);#7;halt_async=1;
  step();if(dut.controller.halt_detected!==0)$fatal(1,"HALT bypassed first stage");
  step();if(dut.controller.halt_detected!==15 || drive_enable!==0)
   $fatal(1,"HALT was not simultaneous at shared second stage");
  step();if(dut.controller.external_latches!==15)$fatal(1,"HALT latches diverged");
  read_word(0,2);
  if(!coast_req)$fatal(1,"software HALT did not request coast");
  if(drive_enable || shared_halt)$fatal(1,"external HALT echoed as local cause");
  @(negedge clk);halt_async=0;repeat(8)step();read_word(0,2);
  if(!coast_req)$fatal(1,"coast cleared before acknowledgment");

  // External HALT cancels cleanup proof without creating a local fault.
  // Recovery now requires fresh cleanup, not just dropping the HALT latch.
  read_word('h1074);if(value!=4)$fatal(1,"HALT retained cleanup proof");
  write_word('h1020,1,0);
  write_word('h1024,1,0);write_word('h1024,2,0);write_word('h1024,4,0);
  write_word('h1020,0,0);wait(dut.cleanup_complete);read_word(0);
  if(coast_req)$fatal(1,"coast did not clear after explicit acknowledgment");
  if(value!=32'h444d4231)$fatal(1,"external HALT recovery");
  // Direct-duty path: no encoder pulses or HLS readiness required.
  direct_start(200000,200000);
  read_word('h1050);if(value!=32'h44445531)$fatal(1,"direct capability");
  write_word('h1054,0,2);write_word('h1058,32768,2);read_word(0,2);
  if(drive_enable)$fatal(1,"nonzero entry duty");
  write_word('h105c,65537,2);write_word('h105c,-65537,2);
  write_word('h105c,16384,0);write_word('h1060,1,0);
  repeat(2200)step();highs=0;repeat(1000)begin step();if(pwm[0])highs=highs+1;end
  if(highs!=250 || drive_enable!=1 || direction[0])$fatal(1,"25 percent duty %d",highs);
  write_word('h1060,1,2); // replay cannot refresh command age
  write_word('h105c,65536,0);write_word('h1060,2,0);
  repeat(1200)step();highs=0;repeat(1000)begin step();if(pwm[0])highs=highs+1;end
  if(highs!=1000)$fatal(1,"100 percent duty");
  write_word('h105c,-32768,0);write_word('h1060,3,0);
  wait(!drive_enable[0]);repeat(3200)step();highs=0;repeat(1000)begin step();if(pwm[0])highs=highs+1;end
  if(highs!=500 || !direction[0])$fatal(1,"reverse duty");
  write_word('h105c,0,0);write_word('h1060,4,0);repeat(1200)step();if(drive_enable)$fatal(1,"zero duty");
  write_word('h105c,16384,0);write_word('h1060,5,0);repeat(1200)step();
  write_word('h1014,1,0);repeat(8)step();if(drive_enable || dut.enabled)$fatal(1,"direct STOP");
  write_word('h1060,6,2);write_word('h100c,1,2);
  direct_start(200000,20000);write_word('h105c,16384,0);write_word('h1060,1,0);
  wait(shared_halt);repeat(5)step();if(drive_enable || dut.age_faults!=1)$fatal(1,"direct command expiry");
  direct_start(20000,200000);write_word('h105c,16384,0);write_word('h1060,1,0);
  wait(shared_halt);repeat(5)step();if(drive_enable || !dut.watchdog_faults[0])$fatal(1,"direct watchdog");
  direct_start(200000,200000);write_word('h105c,16384,0);write_word('h1060,1,0);repeat(1200)step();
  @(negedge clk);physical_faults=2;repeat(8)step();if(drive_enable || !shared_halt)$fatal(1,"direct shared external fault");
  direct_start(200000,200000);write_word('h105c,16384,0);write_word('h1060,1,0);repeat(1200)step();
  @(negedge clk);halt_async=1;repeat(8)step();if(drive_enable || dut.enabled)$fatal(1,"direct HALT");
  direct_start(200000,200000,32768);
  write_word('h105c,32769,2);write_word('h105c,-32769,2);
  write_word('h105c,-32768,0);write_word('h1060,1,0);repeat(3200)step();
  if(drive_enable!=1 || !direction[0])$fatal(1,"configured ceiling boundary");
  // Physical maintenance switch must dominate running direct drive.
  @(negedge clk);maintenance_async=1;repeat(8)step();
  if(pwm || drive_enable || !coast_req)$fatal(1,"maintenance failed to inhibit");
  @(negedge clk);maintenance_async=0;repeat(12)step();
  if(pwm || drive_enable || !coast_req)$fatal(1,"switch release restarted drive");
  write_word('h1024,2,0);repeat(8)step();
  if(!dut.maintenance_inhibit)$fatal(1,"running ACK cleared maintenance");
  read_word('h1004);if(!value[16])$fatal(1,"maintenance fault status");
  write_word('h100c,1,2); // Maintenance rejects enable.
  write_word('h1020,1,0);repeat(8)step();
  if(!dut.maintenance_inhibit)$fatal(1,"core reset cleared maintenance");
  write_word('h1024,2,0);repeat(8)step();
  if(dut.maintenance_inhibit || drive_enable || pwm)$fatal(1,"disabled explicit ACK");
  direct_start(200000,200000,32768);
  write_word('h105c,-32768,0);write_word('h1060,1,0);repeat(3200)step();
  if(dut.maintenance_inhibit || drive_enable!=1)$fatal(1,"explicit maintenance recovery");
  // HB loss during real direct drive inhibits independently of the engine.
  write_word('h106c,1,2); // ACK cannot clear protection while enabled.
  hb_run=0;repeat(55)step();
  if(!coast_req || pwm || drive_enable || dut.enabled || !dut.monitor_fault)
   $fatal(1,"HB loss did not coast and disable");
  read_word('h1068);if(!value[1] || value[2])$fatal(1,"HB status");
  read_word('h1004);if(!value[16] || !value[18])$fatal(1,"HB protection status");
  write_word('h100c,1,2);write_word('h106c,1,2);
  write_word('h1020,1,0);write_word('h106c,1,2); // stale HB cannot ACK
  repeat(8)step();if(!dut.monitor_fault)$fatal(1,"CORE_RESET cleared HB fault");
  hb_run=1;repeat(55)step();
  if(!dut.monitor_healthy || !dut.monitor_fault || !coast_req)$fatal(1,"HB resume auto-cleared fault");
  write_word('h106c,2,2);write_word('h106c,1,0);
  read_word('h1068);if(value[1] || value[2] || !value[0])$fatal(1,"HB ack during core reset");
  write_word('h1020,0,0);wait(dut.cleanup_complete || dut.direct_mode!=0);repeat(3)step();
  if(dut.enabled || pwm || drive_enable || coast_req)$fatal(1,"HB recovery restarted drive");
  read_word('h1064);if(value!=32'h48423131)$fatal(1,"HB capability");
  // At cold boot, a static pin must not permit even direct-mode enable.
  hb_run=0;@(negedge clk);reset=1;repeat(4)step();@(negedge clk);reset=0;
  write_word('h1008,1,0);repeat(8)step();write_word('h1024,2,0);
  cfg[0]=1;for(i=0;i<20;i=i+1)write_word('h2000+i*4,cfg[i],0);
  write_word('h2080,1,0);write_word('h1054,1,0);write_word('h1058,32768,0);
  write_word('h1020,0,0);wait(dut.cleanup_complete || dut.direct_mode!=0);repeat(3)step();
  if(dut.configured!=1 || !dut.protocol_initialized)$fatal(1,"startup test preconditions");
  write_word('h100c,1,2);repeat(90)step();
  if(!dut.monitor_fault || !coast_req)$fatal(1,"HB startup not protected");
  $display("monitor integration PASS: gating, loss, status, protected ACK, reset and no restart");
  $display("direct-duty PASS: 0/25/50/100 percent, signed ceiling, mode lock, replay rejection, STOP, expiry, watchdog, faults and HALT");
  $display("controller_axi PASS: real AXI, both HLS kernels, configuration, four-motor targets, encoder capture, telemetry shared physical fault, explicit recovery and watchdog expiration");$finish;
 end
 initial begin repeat(500000)step();$fatal(1,"timeout fault=%b ready=%h enabled=%h core=%b direct=%h address=%h valid=%b atomic=%b initialized=%b",dut.fault,dut.ready_mask,dut.enabled,dut.core_reset,dut.direct_mode,dut.host_address,dut.host_valid,dut.atomic_owner,dut.protocol_initialized);end
endmodule
