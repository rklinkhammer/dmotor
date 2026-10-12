`timescale 1ns/1ps
module atomic_adapter_tb;
    import dmotor_batch_registers::*;
    localparam TEST_DEADLINE=128;
    reg clk=0;always #(0.5e9/dmotor_timing::FABRIC_HZ)clk=~clk;
    reg reset=1,command_valid=0,response_ready=0;
    wire command_ready,response_valid,response_error,fault;
    reg [159:0] command_words=0;
    reg [351:0] admission_words=0;
    reg [63:0] command_tick=0;
    wire [511:0] response_words;
    wire [31:0] last_latency,maximum_latency;
    reg [511:0] saved_response;
    integer tx=0;
    dmotor_atomic_hls_adapter #(.MAX_CYCLES(TEST_DEADLINE))dut(.*);
    task step;begin @(posedge clk);#1;end endtask
    task transact(input [31:0] op,input [31:0] a,input [31:0] b,input [31:0] c);begin
        wait(command_ready);@(negedge clk);
        command_words={32'd0,c,b,a,op};command_valid=1;step();
        @(negedge clk);command_valid=0;command_words='1; // Captured words must remain immutable.
        wait(response_valid);#1;if(response_error || fault)$fatal(1,"unexpected adapter failure");
        saved_response=response_words;
        repeat(3)begin step();if(!response_valid || response_words!=saved_response || command_ready)
            $fatal(1,"response backpressure");end
        @(negedge clk);response_ready=1;step();@(negedge clk);response_ready=0;tx=tx+1;
    end endtask
    task write_word(input [31:0] addr,input [31:0] value);begin
        transact(1,addr,value,15);if(response_words[31:0]!=1)$fatal(1,"write rejected %h",addr);
    end endtask
    task check_read(input [31:0] addr,input [31:0] expected);begin
        transact(2,addr,0,0);
        if(response_words[31:0]!=1 || response_words[63:32]!=expected)
            $fatal(1,"read %h expected %h actual %h",addr,expected,response_words[63:32]);
    end endtask
    task publish(input [31:0] seq,input [3:0] mask);integer i;begin
        write_word(STAGE_SESSION,7);write_word(STAGE_SEQUENCE,seq);write_word(STAGE_MASK,{28'd0,mask});
        for(i=0;i<4;i=i+1)if(mask[i])begin
            write_word(STAGE_MOTOR_BASE+i*STAGE_MOTOR_STRIDE,21+i);
            write_word(STAGE_MOTOR_BASE+i*STAGE_MOTOR_STRIDE+VELOCITY_MRPM,i%2 ? -200000 : 200000);
        end
        write_word(COMMIT,1);
    end endtask
    integer i;
    initial begin
        admission_words[31:0]=7;admission_words[63:32]=15;admission_words[95:64]=15;
        for(i=0;i<4;i=i+1)begin
            admission_words[(3+i)*32 +:32]=21+i;admission_words[(7+i)*32 +:32]=200000;
        end
        repeat(3)step();@(negedge clk);reset=0;
        transact(0,0,0,0);check_read(IDENTITY,PROFILE_ID);check_read(SESSION,7);
        publish(1,15);publish(2,15);check_read(PENDING_SEQUENCE,2);
        command_tick=64'h123456789abcdef0;
        transact(3,0,0,0);
        if(response_words[31:0]!=1 || response_words[159:128]!=2 || response_words[191:160]!=15 ||
           response_words[255:192]!=command_tick || response_words[319:288]!=32'hfffcf2c0)
            $fatal(1,"dual-port batch capture");
        check_read(APPLIED_SEQUENCE,0);transact(4,7,2,1);check_read(APPLIED_SEQUENCE,2);
        check_read(APPLIED_TICK_LO,32'h9abcdef0);check_read(APPLIED_TICK_HI,32'h12345678);
        // Cancellation invokes HLS invalidation; late retirement cannot succeed.
        publish(3,3);transact(3,0,0,0);transact(5,1,0,0);transact(4,7,3,1);
        if(response_words[31:0])$fatal(1,"late retirement accepted");check_read(APPLIED_SEQUENCE,2);
        // Captured admission is stable despite unrelated producer changes after acceptance.
        wait(command_ready);@(negedge clk);command_words={32'd0,32'd0,32'd0,32'd0,32'd0};
        command_valid=1;step();@(negedge clk);command_valid=0;admission_words[31:0]=99;
        wait(response_valid);@(negedge clk);response_ready=1;step();@(negedge clk);response_ready=0;
        check_read(SESSION,7);admission_words[31:0]=7;
        // Hang the actual core's completion: deadline must fault/reset independently.
        force dut.hls_done=0;
        wait(command_ready);@(negedge clk);command_words={32'd0,32'd0,32'd0,IDENTITY,32'd2};
        command_valid=1;step();@(negedge clk);command_valid=0;
        wait(response_valid);#1;
        if(!fault || !response_error || response_words)$fatal(1,"deadline not fail-closed");
        @(negedge clk);response_ready=1;step();@(negedge clk);response_ready=0;
        if(command_ready)$fatal(1,"fault auto-recovered");release dut.hls_done;
        @(negedge clk);reset=1;step();@(negedge clk);reset=0;
        transact(0,0,0,0);check_read(APPLIED_SEQUENCE,0);
        $display("atomic_adapter PASS: %0d generated-RTL transactions, dual-port memory timing, immutable capture, backpressure, cancellation and timeout",tx);
        $finish;
    end
    initial begin repeat(TEST_DEADLINE*300)step();$fatal(1,"test timeout");end
endmodule
