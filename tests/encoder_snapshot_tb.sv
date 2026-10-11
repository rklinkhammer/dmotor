`timescale 1ns/1ps
module encoder_snapshot_tb;
    reg clk=0;
    always #(0.5e9 / dmotor_timing::FABRIC_HZ) clk=~clk;
    reg reset=1,event_valid=0,event_negative=0,encoder_range_fault=0;
    reg [63:0] event_timestamp=0;
    reg [31:0] encoder_invalid_transitions=0;
    wire [63:0] count,last_edge;
    wire [31:0] direction_epoch,invalid_transitions;
    wire negative,capture_fault;
    dmotor_encoder_snapshot dut(.*);
    task edge_event(input reg sign, input reg [63:0] stamp);
        begin
            @(negedge clk); event_valid=1; event_negative=sign; event_timestamp=stamp;
            @(negedge clk); event_valid=0;
        end
    endtask
    initial begin
        repeat(3) @(negedge clk); reset=0;
        edge_event(0,100); edge_event(0,200);
        if(count!=2 || last_edge!=200 || direction_epoch!=0 || negative) $fatal(1,"forward events");
        edge_event(1,300); edge_event(0,400);
        if(count!=4 || direction_epoch!=2 || negative) $fatal(1,"hidden double reversal");
        encoder_invalid_transitions=1;
        repeat(2) @(negedge clk);
        if(count!=4 || invalid_transitions!=1) $fatal(1,"invalid must not contribute count");
        encoder_range_fault=1;
        repeat(2) @(negedge clk); encoder_range_fault=0;
        repeat(2) @(negedge clk);
        if(!capture_fault) $fatal(1,"capture fault must latch");
        reset=1; @(negedge clk); reset=0;
        if(count!=0 || direction_epoch!=0 || capture_fault) $fatal(1,"epoch reset");
        encoder_invalid_transitions=32'hffffffff;
        repeat(2) @(negedge clk);
        if(!capture_fault) $fatal(1,"diagnostic saturation must fault");
        $display("encoder snapshot: count, direction epochs, invalid exclusion and fault latching passed");
        $finish;
    end
endmodule
