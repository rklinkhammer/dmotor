`timescale 1ns/1ps
module axi_bridge_tb;
    reg clk=0;always #(0.5e9 / dmotor_timing::FABRIC_HZ) clk=~clk;
    reg reset=1;
    reg [31:0] awaddr=0,wdata=0,araddr=0;
    reg [3:0] wstrb=15;
    reg awvalid=0,wvalid=0,arvalid=0,bready=0,rready=0;
    wire awready,wready,arready,bvalid,rvalid;
    wire [1:0] bresp,rresp;
    wire [31:0] rdata;
    wire start,engine_reset,init,bus_valid,bus_write,service_done,fault,quiesced;
    wire [31:0] address,data;wire [3:0] strobes;
    reg ready=0,done=0,error=0,stall=0;
    reg [31:0] result=0;
    wire [7:0] context_value;
    dmotor_axi_bridge #(.CONTEXT_WIDTH(8),.ENGINE_TIMEOUT(32)) dut(
      clk,reset,awaddr,awvalid,awready,wdata,wstrb,wvalid,wready,
      bresp,bvalid,bready,araddr,arvalid,arready,rdata,rresp,rvalid,rready,
      8'h5a,context_value,start,ready,done,engine_reset,init,bus_valid,bus_write,
      address,data,strobes,result,error,service_done,fault,quiesced);
    integer ticks=0,polls=0,writes=0,reads=0;
    reg accepted=0;
    // Delayed ap_ctrl_hs surrogate, independent of AXI. Integrated generated
    // HLS RTL is tested separately; this specifically attacks transport timing.
    always @(posedge clk) begin
        ready<=0;done<=0;
        if(engine_reset) begin ticks<=0;accepted<=0;end
        else if(!stall) begin
            if(start && !accepted) begin
                accepted<=1;ticks<=4;
                if(!bus_valid) polls<=polls+1;
                else if(bus_write) writes<=writes+1;
                else reads<=reads+1;
                if(context_value!=8'h5a) $fatal(1,"context capture");
            end else if(ticks>1) ticks<=ticks-1;
            else if(ticks==1) begin
                ticks<=0;done<=1;ready<=1;
                result<=address ^ 32'h12345678;error<=bus_write && strobes!=15;
            end
            if(!start) accepted<=0;
        end
    end
    task aw(input [31:0] value);begin
        @(negedge clk);awaddr=value;awvalid=1;
        do begin @(posedge clk);end while(!awready);
        @(negedge clk);awvalid=0;
    end endtask
    task wd(input [31:0] value,input [3:0] mask);begin
        @(negedge clk);wdata=value;wstrb=mask;wvalid=1;
        do begin @(posedge clk);end while(!wready);
        @(negedge clk);wvalid=0;
    end endtask
    task ar(input [31:0] value);begin
        @(negedge clk);araddr=value;arvalid=1;
        do begin @(posedge clk);end while(!arready);
        @(negedge clk);arvalid=0;
    end endtask
    integer saved_polls;
    initial begin
        repeat(2) @(negedge clk);reset=0;
        wd(32'h44,15);repeat(20) @(negedge clk);
        if(bvalid || writes!=0 || polls<2 || quiesced) $fatal(1,"W before AW/polling");
        aw(32'h100);wait(bvalid);#1;if(bresp!=0 || writes!=1) $fatal(1,"write response");
        saved_polls=polls;ar(32'h104);wait(rvalid);#1;
        repeat(30) @(negedge clk);
        if(!bvalid || !rvalid || rdata!=(32'h104^32'h12345678) || polls<=saved_polls) $fatal(1,"backpressure/poll starvation");
        bready=1;rready=1;@(negedge clk);bready=0;rready=0;
        aw(32'h108);repeat(15) @(negedge clk);if(bvalid) $fatal(1,"AW without W");
        wd(32'h99,3);wait(bvalid);#1;if(bresp!=2) $fatal(1,"SLVERR");
        @(negedge clk);bready=1;@(negedge clk);bready=0;
        // Engine hang becomes sticky fault and returns an error to queued read.
        stall=1;ar(32'h200);wait(fault);wait(rvalid);#1;
        if(rresp!=2 || !engine_reset || awready || arready) $fatal(1,"timeout recovery");
        $display("AXI bridge: split channels, stalled responses, idle polling, errors, timeout passed");$finish;
    end
    initial begin #100000; $fatal(1,"test timeout");end
endmodule
