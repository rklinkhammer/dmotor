`timescale 1ns/1ps
module monitor_heartbeat_tb;
 reg clk=0;always #5 clk=~clk;
 reg reset=1,core_reset=0,hb_async=0,acknowledge=0;
 wire fault,qualified,healthy;
 dmotor_monitor_heartbeat #(.STARTUP_CYCLES(20),.TIMEOUT_CYCLES(12),
 .QUALIFY_CYCLES(24),.MIN_EDGE_CYCLES(2)) dut(.*);
 task step;begin @(posedge clk);#1;end endtask
 task restart;begin @(negedge clk);reset=1;hb_async=0;acknowledge=0;
 repeat(3)step();@(negedge clk);reset=0;end endtask
 task pulse_train(input integer n);integer i;begin
 for(i=0;i<n;i=i+1)begin @(negedge clk);hb_async=~hb_async;repeat(4)step();end
 end endtask
 initial begin
 restart();repeat(19)step();if(fault || qualified)$fatal(1,"startup grace");
 step();if(!fault || qualified)$fatal(1,"missing first heartbeat");
 acknowledge=1;repeat(25)step();if(!fault)$fatal(1,"static input cleared fault");
 acknowledge=0;pulse_train(10);
 if(!healthy || !fault || qualified)$fatal(1,"resumption cleared latch");
 acknowledge=1;step();acknowledge=0;step();
 if(!qualified || fault)$fatal(1,"explicit recovery");
 core_reset=1;pulse_train(4);if(qualified || !healthy)$fatal(1,"core reset monitoring");
 core_reset=0;if(!healthy)$fatal(1,"health lost across core reset");
 repeat(14)step();if(!fault || qualified)$fatal(1,"stuck heartbeat");
 core_reset=1;repeat(3)step();core_reset=0;if(!fault)$fatal(1,"core reset cleared latch");
 restart();pulse_train(10);if(!qualified)$fatal(1,"initial qualification");
 // Expiry wins both a synchronized edge and acknowledgment at its boundary.
 @(negedge clk);force dut.age=11;force dut.hb_sync=~dut.previous;acknowledge=1;
 step();if(!fault || qualified)$fatal(1,"expiry precedence");
 @(negedge clk);release dut.age;release dut.hb_sync;acknowledge=0;
 restart();pulse_train(10);
 @(negedge clk);force dut.age=0;force dut.hb_sync=~dut.previous;
 step();if(!fault || qualified)$fatal(1,"fast edge not rejected");
 release dut.age;release dut.hb_sync;
 $display("monitor_heartbeat PASS: startup, qualification, loss, sticky recovery, reset and precedence");$finish;
 end
 initial begin #100000;$fatal(1,"timeout");end
endmodule
