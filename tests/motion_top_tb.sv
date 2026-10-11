`timescale 1ns/1ps
module motion_top_tb;
    reg clk=0;
    always #(0.5e9 / dmotor_timing::FABRIC_HZ) clk=~clk;
    reg reset=1, inhibit=1;
    reg [511:0] command_words=0, configuration_words=0;
    reg command_valid=0, completion_ack=0, immediate_stop=0;
    reg [31:0] session=1;
    reg [63:0] timestamp=0;
    reg signed [63:0] position=0;
    reg signed [31:0] velocity_mrpm=0;
    reg velocity_valid=0;
    wire inputs_sampled, result_valid, pwm, drive_enable, direction, deadline_fault;
    wire [511:0] result_words;
    wire [31:0] motion_faults, last_latency, maximum_latency;
    dmotor_motion_top dut(.*);
    always @(posedge clk) timestamp <= timestamp+1;
    integer n, high_count;
    task result;
        begin
            n=0;
            while (!result_valid && n < dmotor_timing::CONTROL_TICK_CYCLES + dmotor_timing::CONTROL_DEADLINE_CYCLES) begin @(negedge clk); n=n+1; end
            if (!result_valid || deadline_fault) $fatal(1,"missing result or deadline fault");
        end
    endtask
    initial begin
        configuration_words[0*32+:32]=1;
        configuration_words[1*32+:32]=130000;
        configuration_words[2*32+:32]=1000000;
        configuration_words[3*32+:32]=1000000;
        configuration_words[4*32+:32]=10296;
        configuration_words[5*32+:32]=5;
        configuration_words[6*32+:32]=1;
        configuration_words[7*32+:32]=100;
        configuration_words[8*32+:32]=2;
        configuration_words[9*32+:32]=655;
        configuration_words[10*32+:32]=65536;
        configuration_words[11*32+:32]=65536;
        configuration_words[12*32+:32]=65536;
        configuration_words[13*32+:32]=1000000;
        command_words[0*32+:32]=4;
        command_words[3*32+:32]=1;
        command_words[4*32+:32]=1;
        command_words[8*32+:32]=32768;
        repeat(4) @(negedge clk);
        reset=0; inhibit=0; command_valid=1;
        result();
        if (result_words[31:0]!=32768 || result_words[7*32+:32]!=0 ||
            (result_words[2*32+:32]&12)!=12) $fatal(1,"open-loop result");
        $display("motion RTL: first result at %0d clocks",timestamp);
        command_valid=0; completion_ack=1;
        repeat(2 * dmotor_timing::PWM_PERIOD_CYCLES) @(negedge clk);
        high_count=0;
        repeat(dmotor_timing::PWM_PERIOD_CYCLES) begin @(negedge clk); if(pwm) high_count=high_count+1; end
        if (high_count != dmotor_timing::PWM_PERIOD_CYCLES / 2 || !drive_enable || direction) $fatal(1,"PWM duty %0d",high_count);
        result();
        if (!(result_words[2*32+:32]&1)) $fatal(1,"completion acknowledgment");
        $display("motion RTL: acknowledgment at %0d clocks",timestamp);
        completion_ack=0;
        // Stop protection mid-transaction; no late result can restore drive.
        wait(inputs_sampled); @(negedge clk); inhibit=1; #1;
        if (pwm || drive_enable) $fatal(1,"inhibit must immediately disable drive");
        repeat(1000) begin
            @(negedge clk);
            if (pwm || drive_enable || result_valid) $fatal(1,"late output after inhibition");
        end
        // A fresh run with a closed-loop command and invalid feedback faults.
        command_words[0*32+:32]=1;
        command_words[5*32+:32]=1000;
        command_words[8*32+:32]=0;
        command_valid=1; inhibit=0;
        result();
        if (motion_faults!=8 || pwm || drive_enable || result_words[5*32+:32]!=2)
            $fatal(1,"invalid feedback must abort, fault and inhibit");
        repeat(200) @(negedge clk);
        if (motion_faults!=8 || drive_enable) $fatal(1,"fault must remain sticky");
        $display("motion generated RTL: scheduled execution, completion, 50%% PWM, mid-flight inhibit and feedback fault passed; maximum latency %0d cycles",maximum_latency);
        $finish;
    end
    initial begin #10000000; $fatal(1,"simulation timeout"); end
endmodule
