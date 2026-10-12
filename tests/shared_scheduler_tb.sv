`timescale 1ns/1ps
module shared_scheduler_tb;
    import dmotor_timing::*;
    // Accelerated handshake tests; production clocks remain generated constants.
    localparam TICK=160, DEADLINE=100, PWM=8, SNAP=32;
    reg clk=0;always #(0.5e9/FABRIC_HZ)clk=~clk;
    reg reset=1,inhibit=0,cancel_frame=0;
    reg [3:0] enabled=15;
    reg [31:0] session=7;
    reg [127:0] generations={32'd24,32'd23,32'd22,32'd21};
    reg [127:0] snapshots={32'd104,32'd103,32'd102,32'd101};
    wire [63:0] timestamp,frame_tick;
    wire admission_request;
    reg admission_done=0,batch_valid=1;
    reg [3:0] batch_mask=15;
    reg [31:0] batch_session=7,batch_sequence=1;
    reg [127:0] batch_generations={32'd24,32'd23,32'd22,32'd21};
    reg [127:0] batch_targets={32'd4000,32'd3000,32'd2000,32'd1000};
    wire engine_start,engine_reset,engine_apply_target;
    reg engine_ready=0,engine_done=0,engine_fault=0;
    wire [1:0] engine_motor;
    wire [31:0] engine_session,engine_generation,engine_snapshot,engine_target;
    reg [1:0] result_motor=0;
    reg [63:0] result_tick=0;
    reg [31:0] result_session=0,result_generation=0;
    reg signed [31:0] result_duty=0;
    wire target_apply;
    wire [3:0] target_mask;
    wire target_accepted,target_inhibit;
    wire retire_request;
    wire [31:0] retire_session,retire_sequence;
    reg retire_done=0,retire_success=1;
    wire frame_aborted,frame_committed,busy,fault_inhibit,fault;
    wire [127:0] committed_duties,pwm_duties;
    wire [31:0] last_latency,maximum_latency;
    reg [3:0] configure=0,stop_or_ready=0;
    wire [3:0] age_faults, drive_enable,pwm,direction;
    wire [127:0] applied_duties;
    dmotor_shared_control #(.SNAPSHOT_BITS(SNAP),.TICK_CYCLES(TICK),
        .DEADLINE_CYCLES(DEADLINE),.PWM_CYCLES(PWM))dut(
        .age_configure(configure),.age_acknowledge(4'd0),
        .age_timeout_cycles(dmotor_timing::TARGET_TIMEOUT_DEFAULT_CYCLES),
        .age_configured(),.age_configure_accepted(),.age_armed(),
        .target_ages(),.target_ticks(),.target_fault_times(),.halt_request(),.*);
    task step;begin @(posedge clk);#1;end endtask
    task setup;begin
        @(negedge clk);reset=1;inhibit=0;cancel_frame=0;enabled=0;session=7;
        generations={32'd24,32'd23,32'd22,32'd21};snapshots={32'd104,32'd103,32'd102,32'd101};
        admission_done=0;batch_valid=1;batch_mask=15;batch_session=7;batch_sequence=1;
        engine_ready=0;engine_done=0;engine_fault=0;retire_done=0;retire_success=1;
        configure=0;stop_or_ready=0;
        step();@(negedge clk);reset=0;configure=15;step();
        @(negedge clk);configure=0;enabled=15;
    end endtask
    task admit;begin
        wait(admission_request);@(negedge clk);admission_done=1;step();
        @(negedge clk);admission_done=0;
    end endtask
    task finish_motor(input integer motor,input integer delay_cycles);begin
        if(!engine_start || engine_motor!=motor) $fatal(1,"selection/start");
        if(engine_snapshot!=101+motor || engine_generation!=21+motor || engine_session!=7)
            $fatal(1,"snapshot/tag capture");
        repeat(2)begin step();if(!engine_start)$fatal(1,"start not held");end
        @(negedge clk);engine_ready=1;step();
        @(negedge clk);engine_ready=0;
        repeat(delay_cycles)step();
        @(negedge clk);result_motor=motor;result_tick=frame_tick;
        result_session=7;result_generation=21+motor;result_duty=(motor+1)*10000;
        engine_done=1;step();@(negedge clk);engine_done=0;
    end endtask
    task finish_frame;integer j;begin
        for(j=0;j<4;j=j+1)begin
            finish_motor(j,j+1);
            if(committed_duties || applied_duties) $fatal(1,"partial frame escaped");
        end
        wait(retire_request);
        if(retire_session!=7 || retire_sequence!=1)$fatal(1,"retirement tags");
        @(negedge clk);retire_done=1;step();@(negedge clk);retire_done=0;
    end endtask
    integer j;
    initial begin
        setup();admit();
        // Snapshot inputs change while the captured frame is being calculated.
        snapshots=0;
        finish_frame();wait(frame_committed);#1;
        if(committed_duties!={32'd40000,32'd30000,32'd20000,32'd10000})$fatal(1,"commit values");
        if(applied_duties!=committed_duties || drive_enable!=15)$fatal(1,"PWM not simultaneous");
        if(last_latency>=DEADLINE || maximum_latency!=last_latency)$fatal(1,"latency accounting");
        // Disable/re-enable cannot replay an old committed duty before a new frame.
        @(negedge clk);enabled=11;step();
        @(negedge clk);enabled=15;
        repeat(PWM)step();
        if(applied_duties[95:64] || drive_enable[2])$fatal(1,"old duty replayed on enable");
        // External fault must bypass the PWM boundary immediately.
        @(negedge clk);inhibit=1;#1;if(drive_enable)$fatal(1,"late external inhibit");step();
        if(committed_duties)$fatal(1,"stale committed duties");
        // Bad tagged result never reaches another motor.
        setup();admit();@(negedge clk);engine_ready=1;step();
        @(negedge clk);engine_ready=0;engine_done=1;result_motor=1;result_tick=frame_tick;
        result_session=7;result_generation=21;result_duty=10000;#1;
        if(!fault_inhibit)$fatal(1,"tag fault not immediate");step();
        if(!fault || committed_duties)$fatal(1,"bad result applied");
        // Cancel an accepted call: drain without resetting peer HLS contexts.
        setup();admit();@(negedge clk);engine_ready=1;step();
        @(negedge clk);engine_ready=0;cancel_frame=1;step();
        if(!frame_aborted || engine_reset)$fatal(1,"cancel reset contexts");
        @(negedge clk);cancel_frame=0;engine_done=1;step();
        @(negedge clk);engine_done=0;
        if(busy || committed_duties || fault)$fatal(1,"canceled result retained");
        // A hung call cannot block independent deadline protection.
        setup();admit();wait(fault);#1;
        if(!fault_inhibit || !engine_reset || committed_duties)$fatal(1,"deadline");
        // A done on the deadline boundary loses.
        setup();admit();@(negedge clk);engine_ready=1;step();@(negedge clk);engine_ready=0;
        wait(dut.scheduler.age==DEADLINE-1);@(negedge clk);engine_done=1;result_motor=0;
        result_tick=frame_tick;result_session=7;result_generation=21;step();
        if(!fault || committed_duties)$fatal(1,"deadline completion won");
        // Retirement rejection discards the entire calculated frame.
        setup();admit();for(j=0;j<4;j=j+1)finish_motor(j,1);
        wait(retire_request);@(negedge clk);retire_success=0;retire_done=1;step();
        if(!fault || committed_duties)$fatal(1,"failed retirement");
        // Timer refusal cannot publish a partial/unauthorized frame.
        setup();admit();stop_or_ready=8;
        for(j=0;j<4;j=j+1)finish_motor(j,1);
        wait(fault);#1;if(retire_request || committed_duties)$fatal(1,"age refusal");
        // Ordinary control ticks do not renew target age or retire a new batch.
        setup();batch_valid=0;enabled=5;admit();finish_motor(0,1);finish_motor(2,1);
        if(target_apply || retire_request)$fatal(1,"control tick renewed target intent");
        wait(frame_committed);#1;
        if(applied_duties!={32'd0,32'd30000,32'd0,32'd10000})$fatal(1,"disabled channel output");
        // Cancellation after successful retirement but before PWM must suppress the frame.
        setup();admit();finish_frame();cancel_frame=1;step();
        if(frame_committed || committed_duties || applied_duties)$fatal(1,"cancel released frame");
        // Live session changes invalidate the result before output release.
        setup();admit();session=8;#1;if(!fault_inhibit)$fatal(1,"session mismatch");step();
        if(!frame_aborted || !fault)$fatal(1,"session cancellation");
        $display("shared_scheduler PASS: captured inputs, four-channel PWM commit, target-age/retirement gating, cancellation, stale tags and deadlines");
        $finish;
    end
    initial begin repeat(TICK*30)step();$fatal(1,"testbench timeout");end
endmodule
