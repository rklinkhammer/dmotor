`timescale 1ns/1ps
module transport_tb;
 reg clk=0;always #5 clk=~clk;
 reg reset=1,ps_power_reset=1,command_valid=0,response_ready=0;
 reg [1119:0] command_record=0;wire command_ready,response_valid,response_error,fault;
 wire [511:0] response_words;wire [31:0] last_latency,maximum_latency;
 reg ps_valid=0,ps_write=0,ps_response_ready=0;reg [15:0] ps_address=0;
 reg [31:0] ps_wdata=0;reg [3:0] ps_strobes=15;
 wire ps_ready,ps_response_valid,ps_response_okay;wire [31:0] ps_rdata;
 dmotor_shared_engine_transport #(.MAX_CYCLES(300)) dut(.*);
 task access(input bit wr,input [15:0] a,input [31:0] d);
 begin
  @(negedge clk);ps_valid=1;ps_write=wr;ps_address=a;ps_wdata=d;
  do @(posedge clk);while(!ps_ready);
  @(negedge clk);ps_valid=0;ps_response_ready=1;
  do @(posedge clk);while(!ps_response_valid);
  if(!ps_response_okay)$fatal(1,"bus error %h",a);
  @(negedge clk);ps_response_ready=0;
 end endtask
 task issue;
 begin
  @(negedge clk);command_valid=1;command_record=1120'h1234;
  do @(posedge clk);while(!command_ready);
  @(negedge clk);command_valid=0;command_record=1120'hffff;
 end endtask
 task stage(input [63:0] t);
 begin
  access(1,'h4018,t[31:0]);access(1,'h401c,t[63:32]);
  for(integer i=0;i<16;i=i+1)access(1,'h4140+4*i,100+i);
 end endtask
 initial begin
  repeat(4)@(negedge clk);ps_power_reset=0;reset=0;
  access(1,'h4024,1);issue;
  access(0,'h4080,0);if(ps_rdata!=32'h1234)$fatal(1,"record mutated");
  access(1,'h4020,1);if(response_valid)$fatal(1,"partial accepted");
  stage(2);access(1,'h4020,1);if(response_valid)$fatal(1,"wrong ticket accepted");
  stage(1);access(1,'h4020,1);if(!response_valid||response_error||response_words[31:0]!=100)$fatal(1,"valid reply");
  repeat(10)@(negedge clk);if(!response_valid||command_ready||response_words[511:480]!=115)$fatal(1,"backpressure");
  response_ready=1;@(negedge clk);response_ready=0;
  issue;stage(2);reset=1;repeat(3)@(negedge clk);reset=0;
  access(1,'h4020,1);if(response_valid)$fatal(1,"cancelled reply accepted");
  issue;stage(2);access(1,'h4020,1);if(response_valid)$fatal(1,"logical reset reused ticket");
  // No PS reply: independent deadline expires even with bus response blocked.
  @(negedge clk);ps_address='h4008;ps_write=0;ps_valid=1;ps_response_ready=0;
  @(negedge clk);ps_valid=0;
  repeat(310)@(negedge clk);if(!fault||!response_error||response_words!=0)$fatal(1,"timeout");
  ps_response_ready=1;@(negedge clk);ps_response_ready=0;
  reset=1;repeat(3)@(negedge clk);reset=0;
  issue;stage(4);
  while(dut.age<299)@(negedge clk);
  ps_address='h4020;ps_wdata=1;ps_write=1;ps_valid=1;
  @(negedge clk);ps_valid=0;
  if(!fault||!response_error||dut.accepted_ticket!=1)$fatal(1,"deadline equality did not win");
  ps_power_reset=1;repeat(3)@(negedge clk);ps_power_reset=0;
  if(command_ready||dut.attached)$fatal(1,"physical reset retained attach");
  $display("PASS: PS transport immutable capture, full mask, stale tickets, backpressure, cancellation, reset, timeout and equality priority");$finish;
 end
 initial begin #100000;$fatal(1,"timeout");end
endmodule
