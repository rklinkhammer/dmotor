`timescale 1ns/1ps
module qcencoder_tb;
    reg clk=0; always #5 clk=~clk;
    reg reset=1,initialize=0,qa=0,qb=0,invert=0;
    reg [1:0] mode=2;
    reg signed [63:0] initial_position=0;
    wire signed [63:0] position;
    wire [31:0] errors;
    wire fault,event_valid,event_negative;
    qcencoder dut(clk,reset,initialize,qa,qb,invert,mode,initial_position,
                  position,errors,fault,event_valid,event_negative,,);
    task state(input [1:0] value); begin
        @(negedge clk); {qa,qb}=value; repeat(1000) @(negedge clk);
    end endtask
    integer m;
    initial begin
        repeat(2) @(negedge clk); reset=0; repeat(5) @(negedge clk);
        for(m=0;m<3;m=m+1) begin
            mode=m; initialize=1; @(negedge clk); initialize=0;
            repeat(5) @(negedge clk);
            state(1);state(3);state(2);state(0);
            if(position != (1<<m)) $fatal(1,"decode mode %d position %d",m,position);
            state(2);state(3);state(1);state(0);
            if(position != 0) $fatal(1,"reverse");
        end
        state(3); if(errors != 1 || position != 0) $fatal(1,"invalid transition");
        initial_position=64'sh7fffffffffffffff; initialize=1;
        @(negedge clk); initialize=0; repeat(5) @(negedge clk);
        state(2); if(!fault || position != initial_position) $fatal(1,"overflow");
        $display("qcencoder: X1/X2/X4, reverse, invalid, saturation at capture envelope passed");
        $finish;
    end
endmodule
