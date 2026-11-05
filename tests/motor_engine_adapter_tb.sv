`timescale 1ns/1ps
// Adapter reset/ordering test only. Transport stub is NOT generated HLS evidence.
module motor_engine_adapter_tb;
 reg clk=0;always #5 clk=~clk;
 reg reset=1,start=0,apply_target=0;
 reg [1:0] motor=0;
 reg [31:0] session=1,generation=1,target=0;
 reg [63:0] tick=1;
 reg [255:0] snapshot=0;
 reg [3:0] enabled=15,stop=0;
 reg [2559:0] configurations=0;
 wire ready,done,fault;
 wire [3:0] ready_mask,stopped;
 wire [1:0] result_motor;
 wire [31:0] result_session,result_generation;
 wire [63:0] result_tick;
 wire signed [31:0] result_duty;
 wire [2047:0] telemetry;
 integer m,phase,before_count;
 dmotor_motor_engine_adapter dut(.*);
 task step;begin @(posedge clk);#1;end endtask
 task launch;begin wait(ready);@(negedge clk);start=1;step();@(negedge clk);start=0;end endtask
 task check_reset;begin
  @(negedge clk);reset=1;step();
  if(done || ready || ready_mask || result_duty || telemetry || dut.initialized || dut.active_targets)
   $fatal(1,"adapter state survived reset");
  // Reset is held: no explicit context-reset transactions can execute.
  before_count=dut.transport.reset_count;
  repeat(4)step();
  if(dut.transport.reset_count!=before_count)$fatal(1,"context reset executed while held");
  @(negedge clk);reset=0;repeat(8)begin step();if(done)$fatal(1,"late completion after reset");end
 end endtask
 task warm;begin
  before_count=dut.transport.reset_count;
  launch();wait(done);#1;
  if(fault || !ready_mask[motor] || result_session!=session || result_duty)
   $fatal(1,"warmup failed");
  if(dut.transport.reset_count!=before_count+1)$fatal(1,"missing explicit context reset");
  step();
 end endtask
 initial begin
  for(m=0;m<4;m=m+1)configurations[m*640 +:32]=1;
  check_reset();
  for(m=0;m<4;m=m+1)begin motor=m;warm();end
  // Cancel at adapter SEND, WAIT_REPLY and FINISH. New session cannot inherit
  // initialized/ready state or an old result; first new invocation resets context.
  for(phase=1;phase<=3;phase=phase+1)begin
   motor=0;launch();wait(dut.state==phase);check_reset();
   session=session+1;warm();
  end
  $display("motor_engine_adapter reset PASS: all contexts initialized lazily; SEND/WAIT_REPLY/FINISH cancellation and no late completion");
  $finish;
 end
 initial begin repeat(3000)step();$fatal(1,"test timeout");end
endmodule

module dmotor_shared_engine_transport(
 input wire clk,reset,command_valid,
 output wire command_ready,
 input wire [1119:0] command_record,
 output reg response_valid=0,
 input wire response_ready,
 output reg [511:0] response_words=0,
 output wire response_error,fault,
 output wire [31:0] last_latency,maximum_latency
);
 reg [3:0] requires_init=15;
 reg [1119:0] saved=0;
 integer delay_left=0;
 integer reset_count=0;
 assign command_ready=!reset && delay_left==0 && !response_valid;
 assign response_error=0;assign fault=0;
 assign last_latency=0;assign maximum_latency=0;
 always @(posedge clk)begin
  if(reset)begin
   delay_left<=0;response_valid<=0;response_words<=0;requires_init<=15;
  end else begin
   if(response_valid && response_ready)response_valid<=0;
   if(command_valid && command_ready)begin saved<=command_record;delay_left<=3;end
   if(delay_left>0)begin
    delay_left<=delay_left-1;
    if(delay_left==1)begin
     response_words<=0;response_words[224]<=1;response_valid<=1;
     if(saved[32])begin
      requires_init[saved[1:0]]<=0;reset_count<=reset_count+1;
     end else if(requires_init[saved[1:0]])$fatal(1,"operation before context reset");
    end
   end
  end
 end
endmodule
