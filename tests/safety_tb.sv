`timescale 1ns/1ps
module safety_tb;
    reg clk=0; always #5 clk=~clk;
    reg reset=1, ack=0;
    reg [3:0] enable_req=0, disable_req=0, ack_local=0, ack_external=0, refresh=0;
    reg [31:0] causes[0:3];
    reg [31:0] sequences[0:3];
    reg [31:0] refresh_sessions[0:3];
    reg [63:0] timeout=100;
    wire [3:0] enabled, inhibit, requests, external_latched;
    wire shared_halt;
    wire [31:0] faults[0:3], expected[0:3];
    wire [63:0] timestamps[0:3], last[0:3];
    genvar g;
    generate for(g=0;g<4;g=g+1) begin: motors
        dmotor_safety dut(.clk(clk),.reset(reset),.halt_async(shared_halt),
          .local_faults(causes[g]),.enable_request(enable_req[g]),.disable_request(disable_req[g]),
          .prerequisites_valid(1'b1),.acknowledge_local(ack_local[g]),
          .acknowledge_external(ack_external[g]),.timeout_cycles(timeout),
          .refresh_valid(refresh[g]),.session(32'd9),.refresh_session(refresh_sessions[g]),
          .refresh_sequence(sequences[g]),.enabled(enabled[g]),.inhibit(inhibit[g]),
          .halt_out(requests[g]),.external_latched(external_latched[g]),.faults(faults[g]),
          .expected_sequence(expected[g]),.timestamp(timestamps[g]),.last_refresh(last[g]),
          .first_fault_timestamp(),.halt_detected(),.acknowledge_mask(32'hffffffff));
    end endgenerate
    dmotor_halt_aggregator aggregator(clk,reset,ack,requests,shared_halt);
    task step; begin @(posedge clk); #1; end endtask
    task clear_all; integer j; begin
        @(negedge clk); reset=1; enable_req=0; ack_local=0; ack_external=0; refresh=0; ack=0;
        for(j=0;j<4;j=j+1) begin causes[j]=0; sequences[j]=1; refresh_sessions[j]=9; end
        step(); @(negedge clk); reset=0; enable_req=15; step();
        @(negedge clk); enable_req=0;
        if(enabled != 15 || inhibit != 0) $fatal(1,"enable");
    end endtask
    integer source, bit_index, k;
    reg [63:0] old_refresh;
    initial begin
        // Every local fault bit, every initiating motor; receiver must not echo.
        for(source=0;source<4;source=source+1) begin
            for(bit_index=1;bit_index<10;bit_index=bit_index+1) begin
                clear_all(); causes[source]=32'b1 << bit_index;
                step(); if(!inhibit[source] || !requests[source]) $fatal(1,"local fault");
                repeat(5) step();
                if(inhibit != 15 || requests != (1 << source)) $fatal(1,"shared fault attribution");
                @(negedge clk); ack_local=15; step();
                if(!requests[source]) $fatal(1,"cleared live source");
                @(negedge clk); causes[source]=0; step();
                @(negedge clk); ack_local=0; ack=1; step();
                @(negedge clk); ack=0; repeat(3) step();
                @(negedge clk); ack_external=15; step();
                @(negedge clk); ack_external=0; repeat(2) step();
                if(enabled != 0 || inhibit != 15) $fatal(1,"automatic restart");
            end
        end
        clear_all(); old_refresh=last[0];
        refresh=1; refresh_sessions[0]=8; step();
        if(last[0] != old_refresh) $fatal(1,"stale session refreshed");
        @(negedge clk); refresh_sessions[0]=9; sequences[0]=2; step();
        if(last[0] != old_refresh) $fatal(1,"wrong sequence refreshed");
        @(negedge clk); sequences[0]=1; step();
        if(expected[0] != 2 || last[0] == old_refresh) $fatal(1,"valid refresh");
        @(negedge clk); refresh=0;
        // All controllers expire at their own timestamps; boundary refresh loses.
        clear_all();
        while(timestamps[0]-last[0] < timeout) step();
        @(negedge clk); refresh=15;
        step();
        if(faults[0] != 1 || enabled != 0) $fatal(1,"expiry precedence");
        repeat(6) step(); if(inhibit != 15) $fatal(1,"watchdog shared halt");
        clear_all();
        // Keep three motors authorized while only motor zero loses authorization.
        while(!requests[0]) begin
            @(negedge clk); refresh=14;
            for(k=1;k<4;k=k+1) sequences[k]=expected[k];
            step();
        end
        repeat(5) step();
        if(requests != 1 || inhibit != 15) $fatal(1,"single watchdog source attribution");
        $display("safety: 36 fault/source cases, recovery, stale refresh, expiry boundary passed");
        $finish;
    end
    initial begin #1000000; $fatal(1,"test timeout"); end
endmodule
