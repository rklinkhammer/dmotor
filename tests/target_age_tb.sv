`timescale 1ns/1ps
module target_age_tb;
    import dmotor_timing::*;
    localparam CHANNELS=4;
    localparam [31:0] TEST_LIMIT=TARGET_TIMEOUT_MIN_CYCLES;
    // Short authorization interval for simulation; target limits use generated units.
    localparam [63:0] WATCHDOG_LIMIT=100;
    localparam [63:0] START_TICK=100, RENEW_TICK=200;
    localparam [63:0] WRAP_START=64'hfffffffffffffff0, WRAP_END=5, WRAP_ELAPSED=21;
    reg clk=0;
    always #(0.5e9/FABRIC_HZ) clk=~clk;
    reg reset=1;
    reg [63:0] timestamp=0, applied_tick=0;
    reg [3:0] enabled=0, stop_or_ready=0, configure=0, acknowledge=0, applied_mask=0;
    reg [31:0] timeout_cycles=TEST_LIMIT;
    reg applied_valid=0;
    wire [3:0] configured, armed, faults, configure_accepted, expired_now;
    wire applied_accepted, inhibit_all, halt_out;
    wire [255:0] age_cycles, last_applied_ticks, fault_timestamps;
    dmotor_target_age dut(.*);
    wire wd_enabled, wd_inhibit, wd_halt;
    wire [31:0] wd_faults, wd_sequence;
    reg wd_refresh=1;
    // A separate authorization watchdog is deliberately refreshed every clock.
    dmotor_safety watchdog(.clk(clk),.reset(reset),.halt_async(1'b0),.local_faults(32'd0),
        .enable_request(1'b1),.disable_request(1'b0),.prerequisites_valid(1'b1),
        .acknowledge_local(1'b0),.acknowledge_external(1'b0),.timeout_cycles(WATCHDOG_LIMIT),
        .refresh_valid(wd_refresh),.session(32'd1),.refresh_session(32'd1),
        .refresh_sequence(wd_sequence),.enabled(wd_enabled),.inhibit(wd_inhibit),
        .halt_out(wd_halt),.external_latched(),.faults(wd_faults),
        .expected_sequence(wd_sequence),.timestamp(),.last_refresh(),.first_fault_timestamp(),
        .halt_detected(),.acknowledge_mask(32'd0));
    wire shared_halt;
    reg aggregator_ack=0;
    dmotor_halt_aggregator aggregator(.clk(clk),.reset(reset),.acknowledge(aggregator_ack),
        .requests(faults),.halted(shared_halt));
    task step; begin @(posedge clk); #1; end endtask
    task setup; begin
        @(negedge clk);reset=1;enabled=0;stop_or_ready=0;configure=0;acknowledge=0;
        applied_valid=0;applied_mask=0;timestamp=0;applied_tick=0;wd_refresh=1;aggregator_ack=0;
        step();@(negedge clk);reset=0;configure=15;timeout_cycles=TEST_LIMIT;
        step();if(configured!=15 || configure_accepted!=15 || armed) $fatal(1,"setup");
        @(negedge clk);configure=0;enabled=15;
    end endtask
    task apply(input [3:0] mask,input [63:0] tick); begin
        @(negedge clk);applied_mask=mask;applied_tick=tick;applied_valid=1;
        step();@(negedge clk);applied_valid=0;
    end endtask
    integer source;
    initial begin
        // No validated configuration means a batch cannot arm a motor.
        step();@(negedge clk);reset=0;enabled=15;apply(15,0);
        if(configured || armed || applied_accepted) $fatal(1,"unconfigured activation");
        setup();
        // READY needs authorization, but no target-age countdown.
        timestamp=TARGET_TIMEOUT_DEFAULT_CYCLES*2;step();
        if(armed || faults || inhibit_all || !wd_enabled) $fatal(1,"READY ages target");
        // Enabled config changes are rejected and cannot extend a running deadline.
        @(negedge clk);configure=15;timeout_cycles=TARGET_TIMEOUT_MAX_CYCLES;step();
        if(configure_accepted) $fatal(1,"live config accepted");
        @(negedge clk);configure=0;timestamp=START_TICK;
        apply(3,START_TICK);if(armed!=3 || !applied_accepted) $fatal(1,"atomic arm");
        @(negedge clk);timestamp=START_TICK+TEST_LIMIT-1;step();
        if(inhibit_all || age_cycles[63:0]!=TEST_LIMIT-1) $fatal(1,"early expiration");
        // Same-boundary activation, stop and acknowledgment lose to expiration.
        @(negedge clk);timestamp=START_TICK+TEST_LIMIT;applied_tick=timestamp;
        applied_valid=1;applied_mask=15;stop_or_ready=15;acknowledge=15;#1;
        if(expired_now!=3 || !inhibit_all) $fatal(1,"expiration not immediate");
        if(!wd_enabled || wd_faults) $fatal(1,"watchdog should remain refreshed");
        step();if(faults!=3 || applied_accepted || armed) $fatal(1,"boundary priority");
        if(fault_timestamps[63:0]!=timestamp) $fatal(1,"fault timestamp");
        @(negedge clk);applied_valid=0;acknowledge=0;stop_or_ready=0;step();
        if(!shared_halt || !halt_out) $fatal(1,"shared HALT");
        apply(15,timestamp);if(applied_accepted || armed) $fatal(1,"automatic restart");
        @(negedge clk);enabled=0;acknowledge=15;step();
        if(faults || !shared_halt) $fatal(1,"local ack cleared aggregator");
        @(negedge clk);acknowledge=0;aggregator_ack=1;step();
        if(shared_halt) $fatal(1,"aggregator release");
        // Fresh valid activation alone also loses exactly at the old deadline.
        setup();timestamp=START_TICK;apply(1,START_TICK);
        @(negedge clk);timestamp=START_TICK+TEST_LIMIT;
        applied_valid=1;applied_mask=1;applied_tick=timestamp;
        step();if(faults!=1 || applied_accepted) $fatal(1,"refresh won expiration");
        // A valid activation one clock earlier really does renew the timer.
        setup();timestamp=START_TICK;apply(1,START_TICK);
        @(negedge clk);timestamp=START_TICK+TEST_LIMIT-1;apply(1,timestamp);
        if(!applied_accepted || faults) $fatal(1,"early renewal rejected");
        @(negedge clk);timestamp=START_TICK+TEST_LIMIT;step();
        if(inhibit_all || age_cycles[63:0]!=1) $fatal(1,"early renewal did not renew");
        // Each initiating motor expires independently and requests global inhibit.
        for(source=0;source<CHANNELS;source=source+1)begin
            setup();timestamp=START_TICK;apply(1<<source,START_TICK);
            @(negedge clk);timestamp=START_TICK+TEST_LIMIT;step();
            if(faults!=(1<<source) || !inhibit_all || !halt_out) $fatal(1,"source attribution");
        end
        setup();timestamp=START_TICK;apply(15,START_TICK);
        @(negedge clk);timestamp=RENEW_TICK;apply(1,RENEW_TICK);
        if(last_applied_ticks[63:0]!=RENEW_TICK || last_applied_ticks[127:64]!=START_TICK) $fatal(1,"peer refreshed");
        @(negedge clk);timestamp=START_TICK+TEST_LIMIT;step();
        if(faults!=14) $fatal(1,"per-motor aging");
        // STOPPING disarms target age; watchdog still expires without refresh.
        setup();apply(15,0);
        @(negedge clk);stop_or_ready=15;step();
        @(negedge clk);stop_or_ready=0;timestamp=TEST_LIMIT*2;wd_refresh=0;
        repeat(WATCHDOG_LIMIT+3)step();
        if(faults || armed || !wd_halt) $fatal(1,"STOPPING watchdog independence");
        // Old/future common ticks must not buy fresh authorization, even initially.
        setup();timestamp=TEST_LIMIT;apply(3,0);
        if(faults!=3 || applied_accepted) $fatal(1,"already stale initial target");
        setup();timestamp=START_TICK;apply(1,START_TICK+1);
        if(faults!=1 || applied_accepted) $fatal(1,"future tick accepted");
        // Modulo wrap uses elapsed time, not absolute ordering.
        setup();timestamp=WRAP_START;apply(1,timestamp);
        @(negedge clk);timestamp=WRAP_END;step();
        if(faults || age_cycles[63:0]!=WRAP_ELAPSED) $fatal(1,"timestamp wrap");
        // Invalid config and unavailable members reject complete activation.
        setup();@(negedge clk);enabled=0;configure=15;timeout_cycles=0;step();
        if(configure_accepted) $fatal(1,"zero timeout");
        @(negedge clk);timeout_cycles=TARGET_TIMEOUT_MAX_CYCLES+1;step();
        if(configure_accepted) $fatal(1,"oversize timeout");
        @(negedge clk);configure=0;enabled=7;apply(15,0);
        if(applied_accepted || armed) $fatal(1,"partial arm");
        apply(0,0);if(applied_accepted) $fatal(1,"empty mask");
        // Verify the actual approved 100 ms default at its exact boundary.
        setup();@(negedge clk);enabled=0;configure=15;timeout_cycles=TARGET_TIMEOUT_DEFAULT_CYCLES;step();
        @(negedge clk);configure=0;enabled=15;timestamp=0;apply(15,0);
        @(negedge clk);timestamp=TARGET_TIMEOUT_DEFAULT_CYCLES-1;step();
        if(inhibit_all) $fatal(1,"default early");
        @(negedge clk);timestamp=TARGET_TIMEOUT_DEFAULT_CYCLES;step();
        if(faults!=15) $fatal(1,"default late");
        $display("target_age PASS: lifecycle, atomic renewal, expiration priority, four-motor attribution, watchdog independence, wrap and configuration");
        $finish;
    end
endmodule
