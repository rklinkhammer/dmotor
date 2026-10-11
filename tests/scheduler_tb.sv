`timescale 1ns/1ps
module scheduler_tb;
    reg clk=0; always #(0.5e9 / dmotor_timing::FABRIC_HZ) clk=~clk;
    reg reset=1,inhibit=0,ready=0,done=0;
    wire start,busy,valid,fault,engine_reset;
    wire [31:0] latency,maximum;
    dmotor_scheduler #(.TICK_CYCLES(20),.DEADLINE_CYCLES(12)) dut(
      clk,reset,inhibit,ready,done,start,busy,valid,fault,engine_reset,,latency,maximum);
    task step; begin @(posedge clk); #1; end endtask
    initial begin
        step(); @(negedge clk); reset=0;
        wait(start); repeat(3) step(); if(!start) $fatal(1,"start not held");
        @(negedge clk); ready=1; step(); if(start) $fatal(1,"start handshake");
        @(negedge clk); ready=0; done=1; step();
        if(!valid || busy || latency != 5) $fatal(1,"result latency");
        @(negedge clk); done=0;
        wait(start); @(negedge clk); ready=1;
        repeat(11) step();
        @(negedge clk); done=1; step();
        if(!fault || valid || !engine_reset) $fatal(1,"deadline boundary");
        @(negedge clk); done=0; inhibit=1; step();
        if(busy || start || valid) $fatal(1,"inhibit drain");
        $display("scheduler: handshake, result, deadline priority, inhibit passed"); $finish;
    end
    initial begin #100000; $fatal(1,"test timeout"); end
endmodule
