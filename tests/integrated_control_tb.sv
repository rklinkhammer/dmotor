`timescale 1ns/1ps
module integrated_control_tb;
    import dmotor_batch_registers::*;
    localparam TICK=20000, DEADLINE=16000, PWM=1000;
    reg clk=0; always #(0.5e9/dmotor_timing::FABRIC_HZ) clk=~clk;
    reg reset=1,inhibit=0,cancel_frame=0;
    reg [3:0] enabled=0,authorized=15,invalidate_mask=0;
    reg [31:0] session=7;
    reg [127:0] generations={32'd24,32'd23,32'd22,32'd21};
    reg [127:0] max_speeds={4{32'd200000}};
    reg [1023:0] snapshots=0;
    wire protocol_initialized,protocol_fault;
    wire [3:0] ready_mask,stopped;
    wire [2047:0] telemetry;
    reg [2559:0] configurations=0;
    reg [3:0] stop=0;
    reg host_valid=0,host_write=0,host_response_ready=0;
    reg [31:0] host_address=0,host_wdata=0;
    reg [3:0] host_strobes=15;
    wire host_ready,host_response_valid,host_response_okay;
    wire [31:0] host_rdata;
    wire [63:0] timestamp,frame_tick;
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
    dmotor_integrated_control #(.SNAPSHOT_BITS(256),.TICK_CYCLES(TICK),
        .DEADLINE_CYCLES(DEADLINE),.PWM_CYCLES(PWM)) dut(.*);
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
    integer m;
    reg [639:0] cfg;
    initial begin
        cfg=0;
        cfg[0*32 +:32]=21;cfg[1*32 +:32]=200000;
        cfg[2*32 +:32]=1000000;cfg[3*32 +:32]=1000000;
        cfg[4*32 +:32]=10296;cfg[5*32 +:32]=5;
        cfg[6*32 +:32]=1;cfg[7*32 +:32]=100;cfg[8*32 +:32]=2;
        cfg[9*32 +:32]=655;cfg[10*32 +:32]=65536;
        cfg[11*32 +:32]=65536;cfg[12*32 +:32]=65536;
        cfg[13*32 +:32]=1000000;cfg[14*32 +:32]=40000;
        cfg[15*32 +:32]=20000;cfg[16*32 +:32]=128;
        for(m=0;m<4;m=m+1)begin cfg[31:0]=21+m;configurations[m*640 +:640]=cfg;end
        repeat(4)step();@(negedge clk);reset=0;age_configure=15;step();
        @(negedge clk);age_configure=0;wait(protocol_initialized);
        @(negedge clk);enabled=15;
        wait(ready_mask==15);if(fault)$fatal(1,"warmup fault");
        publish(1);wait(frame_committed && dut.control.control.scheduler.saved_sequence==1);#1;
        if(fault || age_armed!=15 || committed_duties==0)$fatal(1,"real engine batch failed");
        check_read(APPLIED_SEQUENCE,1);
        for(m=0;m<4;m=m+1)if(telemetry[m*512+64 +:32]!=1)$fatal(1,"motor target not accepted");
        if(last_latency>=DEADLINE)$fatal(1,"deadline");
        $display("integrated_control: actual four-motor frame latency %0d cycles",last_latency);
        @(negedge clk);stop=15;stop_or_ready=15;invalidate_mask=15;step();
        @(negedge clk);invalidate_mask=0;
        wait(stopped==15);wait(frame_committed);#1;
        if(committed_duties || applied_duties || fault)$fatal(1,"STOP did not reach zero drive");
        @(negedge clk);inhibit=1;#1;if(drive_enable)$fatal(1,"inhibit bypass");
        $display("integrated_control PASS: actual mailbox and motor HLS, warmup, atomic target acceptance, PWM, STOP and inhibit");$finish;
    end
    initial begin repeat(TICK*30)step();$fatal(1,"timeout ready=%h fault=%b",ready_mask,fault);end
endmodule
