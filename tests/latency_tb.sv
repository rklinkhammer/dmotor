`timescale 1ns/1ps
module latency_tb;
    reg clk=0; always #5 clk=~clk;
    reg reset=1,halt=0,enable_request=0;
    wire inhibit,enabled;
    dmotor_safety dut(.clk(clk),.reset(reset),.halt_async(halt),.local_faults(32'd0),
      .enable_request(enable_request),.disable_request(1'b0),.prerequisites_valid(1'b1),
      .acknowledge_local(1'b0),.acknowledge_external(1'b0),.timeout_cycles(64'd500000),
      .refresh_valid(1'b0),.session(32'd1),.refresh_session(32'd1),.refresh_sequence(32'd0),
      .enabled(enabled),.inhibit(inhibit),.halt_out(),.external_latched(),.faults(),
      .expected_sequence(),.timestamp(),.last_refresh(),.first_fault_timestamp(),.halt_detected(),.acknowledge_mask(32'hffffffff));
    integer offset;
    time began;
    initial begin
        // Sweep phase in 1 ns increments against a 10 ns clock. Digital only.
        for(offset=0;offset<10;offset=offset+1) begin
            @(negedge clk); reset=1; halt=0; enable_request=0;
            @(negedge clk); reset=0; enable_request=1;
            @(negedge clk); enable_request=0;
            if(!enabled || inhibit) $fatal(1,"arming");
            #(offset); halt=1; began=$time;
            wait(inhibit); if($time-began > 50) $fatal(1,"HALT > 50ns");
            repeat(5) @(negedge clk);
        end
        $display("latency: 10 asynchronous phases <=50ns digital HALT bound passed"); $finish;
    end
    initial begin #100000; $fatal(1,"timeout"); end
endmodule
