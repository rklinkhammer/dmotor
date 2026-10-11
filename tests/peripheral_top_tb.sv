`timescale 1ns/1ps
module peripheral_top_tb;
    reg clk=0;always #5 clk=~clk;
    reg reset=1;
    reg [31:0] awaddr=0,wdata=0,araddr=0;reg [3:0] wstrb=15;
    reg awvalid=0,wvalid=0,arvalid=0,bready=0,rready=0;
    wire awready,wready,arready,bvalid,rvalid;wire [1:0] bresp,rresp;wire [31:0] rdata;
    reg [63:0] timestamp=0;always @(posedge clk) timestamp<=timestamp+1;
    reg executor_ready=0,cancel=0;
    wire cancel_ack,completion_ack,activated,disable_request,stop_request,fault,quiesced;
    wire [31:0] epoch,seq,status;
    wire enabled,inhibit,halt_out;wire [31:0] safety_faults;
    integer activations=0;always @(posedge clk) if(activated) activations<=activations+1;
    dmotor_peripheral_top dut(clk,reset,awaddr,awvalid,awready,wdata,wstrb,wvalid,wready,
      bresp,bvalid,bready,araddr,arvalid,arready,rdata,rresp,rvalid,rready,
      timestamp,executor_ready,64'd0,32'd130000,1'b1,32'd7,cancel,32'd8,
      1'b0,32'd0,32'd0,32'd0,cancel_ack,completion_ack,activated,epoch,seq,status,
      disable_request,stop_request,fault,quiesced,1'b0,32'd0,enabled,inhibit,halt_out,safety_faults);
    integer start_cycle,max_write=0,max_read=0;
    task wr(input [31:0] address,input [31:0] value,input [3:0] mask,input [1:0] response);begin
        // W arrives before AW, preserving both handshakes independently.
        @(negedge clk);wvalid=1;wdata=value;wstrb=mask;
        do begin @(posedge clk);end while(!wready);
        @(negedge clk);wvalid=0;repeat(2) @(negedge clk);awaddr=address;awvalid=1;
        do begin @(posedge clk);end while(!awready);
        start_cycle=timestamp;@(negedge clk);awvalid=0;
        wait(bvalid);#1;
        if(timestamp-start_cycle>max_write) max_write=timestamp-start_cycle;
        if(timestamp-start_cycle>dmotor_timing::MMIO_RESPONSE_MAX_CYCLES) $fatal(1,"write latency budget");
        if(bresp!=response) $fatal(1,"write %h response %d expected %d",address,bresp,response);
        @(negedge clk);bready=1;@(negedge clk);bready=0;
    end endtask
    task rd(input [31:0] address,input [31:0] value);begin
        @(negedge clk);araddr=address;arvalid=1;
        do begin @(posedge clk);end while(!arready);
        start_cycle=timestamp;@(negedge clk);arvalid=0;wait(rvalid);#1;
        if(timestamp-start_cycle>max_read) max_read=timestamp-start_cycle;
        if(timestamp-start_cycle>dmotor_timing::MMIO_RESPONSE_MAX_CYCLES) $fatal(1,"read latency budget");
        if(rresp!=0 || rdata!=value) $fatal(1,"read %h got %h expected %h",address,rdata,value);
        repeat(3) @(negedge clk);if(rdata!=value || !rvalid) $fatal(1,"response hold");
        rready=1;@(negedge clk);rready=0;
    end endtask
    integer i;
    initial begin
        repeat(5) @(negedge clk);reset=0;
        rd(0,32'h444d4f54);rd(12,dmotor_timing::FABRIC_HZ);
        wr('h40,1000,15,0);wr('h50,1,15,0);
        wr('h120,1,3,2);wr('h120,1,15,0);
        rd('h100,1);
        for(i=0;i<16;i=i+1) wr('h1000+i*4,i==0?1:i==3?1:i==4?1:i==5?1000:0,15,0);
        wr('h10c,1,15,0);wr('h110,1,15,0);
        wr('h1014,2,15,2);
        wr('h20,1,15,0);
        @(negedge clk);executor_ready=1;
        wait(activated);#1;if(epoch!=1 || seq!=0) $fatal(1,"activation");
        rd('h3014,1000);
        wr('h40,2000,15,2); // enabled settings cannot change
        wr('h44,1,15,0);wr('h48,1,15,0);rd('h4c,2);
        wr('h48,1,15,2); // repeated refresh is stale
        @(negedge clk);cancel=1;executor_ready=0;wait(cancel_ack);
        @(negedge clk);cancel=0;
        rd('h2008,2);rd('h3114,8);
        if(activations!=1 || fault) $fatal(1,"duplicate activation or fault");
        wr('h24,1,15,0);
        wr('h118,1,15,0);wr('h124,1,15,0);wr('h120,1,15,0);rd('h100,2);
        // Protection arrives after an idle poll has sampled authorization.
        for(i=0;i<16;i=i+1) wr('h1000+i*4,i==0?1:i==3?2:i==4?1:i==5?1000:0,15,0);
        wr('h10c,2,15,0);wr('h110,1,15,0);wr('h20,1,15,0);
        @(negedge clk);executor_ready=1;
        wait(dut.start && !dut.bus_valid && dut.ctx_ready);
        repeat(2) @(negedge clk);cancel=1;executor_ready=0;
        wait(cancel_ack);@(negedge clk);cancel=0;
        rd('h2008,2);
        if(activations!=1) $fatal(1,"stale in-flight activation escaped cancellation");
        wait(safety_faults[0]);
        if(enabled || !inhibit || !halt_out) $fatal(1,"watchdog not protective");
        rd('h30,1);wr('h48,2,15,2); // late refresh cannot resume
        wr('h34,1,15,0);rd('h30,0);
        if(enabled || !inhibit) $fatal(1,"acknowledgment restarted motor");
        $display("integrated AXI/HLS: PASS; max observed write=%0d read=%0d cycles",max_write,max_read);
        $finish;
    end
    initial begin #10000000;$fatal(1,"timeout");end
endmodule
