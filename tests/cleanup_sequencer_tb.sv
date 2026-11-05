`timescale 1ns/1ps
module cleanup_sequencer_tb;
 reg clk=0;always #5 clk=~clk;
 reg reset=1,inhibit=0,component_fault=0,invalidate=0;
 reg [3:0] enabled=0;
 reg [31:0] session=1;
 reg [2559:0] configurations=0;
 reg mailbox_quiescent=0,engine_ready=1,engine_done=0;
 wire hold_outputs,engine_override,engine_start,complete,pending,admission_ready,failed,local_fault;
 wire [31:0] completed_session;
 dmotor_cleanup_sequencer #(.MAX_CYCLES(30)) dut(.*);
 task step;begin @(posedge clk);#1;end endtask
 task restart;begin
  @(negedge clk);reset=1;inhibit=0;enabled=0;component_fault=0;invalidate=0;engine_done=0;
  mailbox_quiescent=0;step();@(negedge clk);reset=0;
 end endtask
 task launch;begin
  repeat(3)step();if(!hold_outputs || engine_start || admission_ready)$fatal(1,"early admission");
  @(negedge clk);mailbox_quiescent=1;
  wait(engine_start);step();@(negedge clk);mailbox_quiescent=0;
 end endtask
 integer kind;
 initial begin
  restart();launch();
  engine_done=1;step();engine_done=0;repeat(3)step();
  if(complete || !hold_outputs)$fatal(1,"ignored mailbox drain");
  @(negedge clk);mailbox_quiescent=1;step();
  if(!complete || !admission_ready || hold_outputs || completed_session!=session)$fatal(1,"missing completion");
  @(negedge clk);enabled=1;#1;if(complete)$fatal(1,"enabled proof remains valid");
  step();enabled=0;step();if(complete || !admission_ready)$fatal(1,"completion replay after disable");
  // Each change wins over the final completion edge.
  for(kind=0;kind<6;kind=kind+1)begin
   restart();launch();@(negedge clk);engine_done=1;mailbox_quiescent=1;
   case(kind)
    0:session=session+1;
    1:configurations[63]=!configurations[63];
    2:enabled=1;
    3:inhibit=1;
    4:component_fault=1;
    5:invalidate=1;
   endcase
   step();if(!failed || complete || admission_ready || !hold_outputs)$fatal(1,"invalid cleanup accepted");
   if(local_fault!==(kind!=3))$fatal(1,"wrong local fault attribution");
  end
  restart();launch();repeat(32)step();
  if(!failed || complete)$fatal(1,"unbounded cleanup");
  restart();launch();restart();engine_done=1;step();
  if(complete || admission_ready)$fatal(1,"old completion survived reset");
  $display("cleanup_sequencer PASS: drain, inhibition, validity, races, timeout and reset");$finish;
 end
 initial begin repeat(600)step();$fatal(1,"test timeout");end
endmodule
