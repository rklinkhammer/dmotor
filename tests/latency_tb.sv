`timescale 1ns/1ps
module latency_tb;
    localparam integer PHASE_SAMPLES = 10;
    localparam real CLOCK_PERIOD_NS = 1.0e9 / dmotor_timing::FABRIC_HZ;
    localparam real HALT_BUDGET_NS = dmotor_timing::EXTERNAL_HALT_BUDGET_US * 1000.0;
    localparam real PIPELINE_BOUND_NS =
        dmotor_timing::EXTERNAL_HALT_PIPELINE_MAX_CYCLES * CLOCK_PERIOD_NS;
    localparam real TEST_TIMEOUT_NS = PHASE_SAMPLES * 100 * CLOCK_PERIOD_NS;
    localparam [63:0] TEST_WATCHDOG_CYCLES = dmotor_timing::FABRIC_HZ / 200;
    reg clk=0; always #(CLOCK_PERIOD_NS/2.0) clk=~clk;
    reg reset=1,halt=0,enable_request=0;
    wire inhibit,enabled;
    dmotor_safety dut(.clk(clk),.reset(reset),.halt_async(halt),.local_faults(32'd0),
      .enable_request(enable_request),.disable_request(1'b0),.prerequisites_valid(1'b1),
      .acknowledge_local(1'b0),.acknowledge_external(1'b0),.timeout_cycles(TEST_WATCHDOG_CYCLES),
      .refresh_valid(1'b0),.session(32'd1),.refresh_session(32'd1),.refresh_sequence(32'd0),
      .enabled(enabled),.inhibit(inhibit),.halt_out(),.external_latched(),.faults(),
      .expected_sequence(),.timestamp(),.last_refresh(),.first_fault_timestamp(),.halt_detected(),.acknowledge_mask(32'hffffffff));
    integer offset;
    realtime began;
    initial begin
        // Sweep ten relative phases at the selected clock. Digital only.
        for(offset=0;offset<PHASE_SAMPLES;offset=offset+1) begin
            @(negedge clk); reset=1; halt=0; enable_request=0;
            @(negedge clk); reset=0; enable_request=1;
            @(negedge clk); enable_request=0;
            if(!enabled || inhibit) $fatal(1,"arming");
            #(offset * CLOCK_PERIOD_NS / PHASE_SAMPLES); halt=1; began=$realtime;
            wait(inhibit);
            if($realtime-began > HALT_BUDGET_NS) $fatal(1,"HALT exceeds physical-time budget");
            if($realtime-began > PIPELINE_BOUND_NS) $fatal(1,"HALT pipeline regression");
            repeat(dmotor_timing::EXTERNAL_HALT_PIPELINE_MAX_CYCLES) @(negedge clk);
        end
        $display("latency: %0d phases, HALT <=%0g ns budget and <=%0g ns pipeline bound passed",
                     PHASE_SAMPLES, HALT_BUDGET_NS, PIPELINE_BOUND_NS); $finish;
    end
    initial begin #(TEST_TIMEOUT_NS); $fatal(1,"timeout"); end
endmodule
