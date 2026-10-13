`timescale 1ns/1ps
module mailbox_control_tb;
    import dmotor_batch_registers::*;
    localparam TICK=2000, DEADLINE=1500, PWM=20;
    reg clk=0; always #(0.5e9/dmotor_timing::FABRIC_HZ) clk=~clk;
    reg reset=1,inhibit=0,cancel_frame=0;
    reg [3:0] enabled=0,authorized=15,ready_mask=15,invalidate_mask=0;
    reg [31:0] session=7;
    reg [127:0] generations={32'd24,32'd23,32'd22,32'd21};
    reg [127:0] max_speeds={4{32'd200000}};
    reg [127:0] snapshots=0;
    wire protocol_initialized,protocol_fault;
    reg host_valid=0,host_write=0,host_response_ready=0;
    reg [31:0] host_address=0,host_wdata=0;
    reg [3:0] host_strobes=15;
    wire host_ready,host_response_valid,host_response_okay;
    wire [31:0] host_rdata;
    wire [63:0] timestamp,frame_tick;
    wire engine_start,engine_reset,engine_apply_target;
    wire [1:0] engine_motor;
    wire [31:0] engine_session,engine_generation,engine_target,engine_snapshot;
    reg engine_ready=0,engine_done=0,engine_fault=0;
    reg [1:0] result_motor=0;
    reg [63:0] result_tick=0;
    reg [31:0] result_session=0,result_generation=0;
    reg signed [31:0] result_duty=0;
    wire target_apply,target_accepted,target_inhibit;
    wire [3:0] target_mask;
    wire frame_aborted,frame_committed,busy,fault_inhibit,fault;
    wire [127:0] committed_duties,pwm_duties,applied_duties;
    wire [31:0] last_latency,maximum_latency;
    reg [3:0] age_configure=0,age_acknowledge=0,stop_or_ready=0;
    reg [31:0] age_timeout_cycles=dmotor_timing::TARGET_TIMEOUT_DEFAULT_CYCLES;
    wire [3:0] age_configured,age_configure_accepted,age_armed,age_faults;
    wire [255:0] target_ages,target_ticks,target_fault_times;
    wire halt_request;
    wire [3:0] pwm,drive_enable,direction;
    dmotor_mailbox_control #(.SNAPSHOT_BITS(32),.TICK_CYCLES(TICK),
        .DEADLINE_CYCLES(DEADLINE),.PWM_CYCLES(PWM)) dut(.*);
    // Motor calculations only are behavioral. Mailbox, arbitration, scheduler,
    // age acceptance and PWM are the actual integration modules.
    reg [127:0] motor_duties=0;
    reg engine_pending=0;
    integer starts=0,commits=0,begins=0,retires=0;
    always @(posedge clk) begin
        engine_done<=0;engine_ready<=0;
        if(reset || engine_reset)begin engine_pending<=0;motor_duties<=0;end
        else if(engine_pending)begin engine_pending<=0;engine_done<=1;end
        else if(engine_start && !engine_ready && !engine_done)begin
            engine_ready<=1;engine_pending<=1;starts<=starts+1;
            result_motor<=engine_motor;result_tick<=frame_tick;result_session<=engine_session;
            result_generation<=engine_generation;
            result_duty<=engine_apply_target ? engine_target : motor_duties[engine_motor*32 +:32];
            if(engine_apply_target)motor_duties[engine_motor*32 +:32]<=engine_target;
        end
        if(frame_committed)commits<=commits+1;
        if(dut.protocol.accept_command && dut.protocol.selected_owner==2)begins<=begins+1;
        if(dut.protocol.accept_command && dut.protocol.selected_owner==3)retires<=retires+1;
    end
    task step;begin @(posedge clk);#1;end endtask
    task request(input bit wr,input [31:0] addr,input [31:0] value);begin
        @(negedge clk);host_write=wr;host_address=addr;host_wdata=value;host_valid=1;
        @(posedge clk);while(!host_ready)@(posedge clk);#1;
        @(negedge clk);host_valid=0;
    end endtask
    task response;begin
        wait(host_response_valid);#1;
        if(!host_response_okay)$fatal(1,"host operation rejected at %h",host_address);
        @(negedge clk);host_response_ready=1;step();@(negedge clk);host_response_ready=0;
    end endtask
    task write_word(input [31:0] addr,input [31:0] value);begin request(1,addr,value);response();end endtask
    task check_read(input [31:0] addr,input [31:0] value);begin
        request(0,addr,0);wait(host_response_valid);#1;
        if(host_rdata!=value)$fatal(1,"read %h got %h wanted %h",addr,host_rdata,value);
        response();
    end endtask
    task publish(input [31:0] seq);integer m;begin
        write_word(STAGE_SESSION,7);write_word(STAGE_SEQUENCE,seq);write_word(STAGE_MASK,15);
        for(m=0;m<4;m=m+1)begin
            write_word(STAGE_MOTOR_BASE+m*8,21+m);write_word(STAGE_MOTOR_BASE+m*8+4,(m+1)*1000);
        end
        write_word(COMMIT,1);
    end endtask
    task setup;begin
        @(negedge clk);reset=1;enabled=0;inhibit=0;cancel_frame=0;invalidate_mask=0;
        session=7;generations={32'd24,32'd23,32'd22,32'd21};
        host_valid=0;host_response_ready=0;age_configure=0;
        repeat(3)step();@(negedge clk);reset=0;age_configure=15;step();
        @(negedge clk);age_configure=0;wait(protocol_initialized);
        @(negedge clk);enabled=15;repeat(100)step();
    end endtask
    integer saved_begins,saved_retires,saved_commits;
    reg [63:0] applied_tick;
    initial begin
        repeat(4)step();@(negedge clk);reset=0;age_configure=15;step();
        @(negedge clk);age_configure=0;
        wait(protocol_initialized);@(negedge clk);enabled=15;
        wait(dut.protocol.quiescent);repeat(3)step();
        publish(1);
        // Leave a completed host read backpressured across the entire frame.
        request(0,IDENTITY,0);wait(host_response_valid);#1;
        if(host_rdata!=PROFILE_ID)$fatal(1,"identity");
        saved_begins=begins;saved_retires=retires;
        wait(frame_committed);#1;applied_tick=frame_tick;
        if(committed_duties!={32'd4000,32'd3000,32'd2000,32'd1000} || applied_duties!=committed_duties)
            $fatal(1,"atomic output release");
        if(begins!=saved_begins+1 || retires!=saved_retires+1 || starts!=4)
            $fatal(1,"duplicate scheduler transaction");
        if(!host_response_valid || host_rdata!=PROFILE_ID || fault || !age_armed)
            $fatal(1,"host backpressure blocked/corrupted frame");
        response();check_read(APPLIED_SEQUENCE,1);check_read(APPLIED_TICK_LO,applied_tick[31:0]);
        // No batch: continue existing target without renewing its age or retiring.
        saved_retires=retires;wait(frame_committed);step();
        if(retires!=saved_retires || target_ticks[63:0]!=applied_tick)$fatal(1,"empty tick renewed target");
        // Cancel a begin after the generated HLS invocation has started.
        publish(2);wait(dut.protocol.active && dut.protocol.owner==2);
        @(negedge clk);cancel_frame=1;step();@(negedge clk);cancel_frame=0;
        wait(dut.protocol.quiescent);repeat(3)step();
        check_read(INFLIGHT_SEQUENCE,0);check_read(PENDING_SEQUENCE,0);check_read(APPLIED_SEQUENCE,1);
        if(fault)$fatal(1,"begin cancellation unnecessarily faulted");
        // A generation change during reservation must prevent stale activation.
        publish(3);wait(dut.protocol.active && dut.protocol.owner==2);
        saved_commits=commits;@(negedge clk);generations[31:0]=99;step();
        repeat(100)step();
        if(commits!=saved_commits || drive_enable)$fatal(1,"ownership race escaped gate");
        // Scheduler's own ownership check escalates this to shared HALT.
        if(!fault || !halt_request)$fatal(1,"ownership race not escalated");
        // Cancellation during retire cannot leak an applied success to the host
        // or permit a PWM update, even if the C++ core already changed telemetry.
        setup();publish(1);wait(dut.protocol.active && dut.protocol.owner==3);
        wait(dut.protocol.response_valid); // cancel on the response-consumption edge
        saved_commits=commits;@(negedge clk);cancel_frame=1;step();
        @(negedge clk);cancel_frame=0;wait(protocol_fault);repeat(20)step();
        if(commits!=saved_commits || drive_enable || host_ready || !halt_request)
            $fatal(1,"retirement cancellation escaped");
        // A prospectively invalid target timestamp must inhibit immediately without
        // feeding protocol cleanup back into target_apply and oscillating.
        setup();publish(1);wait(target_apply);#1;
        force dut.control.target_age.applied_tick=64'h4000000000000000;
        #1;
        if(target_inhibit!==1 || fault_inhibit!==1 || target_apply!==1 || drive_enable!==0)
            $fatal(1,"target-age cleanup feedback or late inhibit");
        step();release dut.control.target_age.applied_tick;
        if(!age_faults || !halt_request || committed_duties || frame_committed)
            $fatal(1,"invalid target timestamp escaped protection");
        // Timeout in an ordinary host call is also a shared protection event.
        setup();force dut.protocol.transport.hls_done=0;
        request(0,IDENTITY,0);wait(host_response_valid);#1;
        if(host_response_okay || !protocol_fault || !halt_request || drive_enable)
            $fatal(1,"transport timeout not escalated");
        release dut.protocol.transport.hls_done;
        // Session changes require a drained reset, never an implicit reinitialization.
        setup();@(negedge clk);session=8;step();
        if(!protocol_fault || host_ready || drive_enable)$fatal(1,"session silently replaced");
        $display("mailbox_control PASS: generated HLS admission/retirement, host backpressure, atomic PWM, no-batch tick, cancellation, ownership/retirement races, timeout and session protection");
        $finish;
    end
    initial begin repeat(TICK*20)step();$fatal(1,"test timeout active=%b owner=%d init=%b fault=%b pending=%h scheduler=%d hostv=%b",dut.protocol.active,dut.protocol.owner,protocol_initialized,fault,dut.protocol.pending_invalidate,dut.control.scheduler.state,host_valid);end
endmodule
