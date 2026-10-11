`timescale 1ns/1ps
module pwm_tb;
    reg clk=0; always #(0.5e9 / dmotor_timing::FABRIC_HZ) clk=~clk;
    reg reset=1,inhibit=0;
    reg signed [31:0] duty=32768;
    wire pwm,enabled,direction,reversing;
    wire signed [31:0] applied;
    dmotor_pwm #(.PERIOD_CYCLES(20)) dut(clk,reset,inhibit,duty,pwm,enabled,direction,reversing,applied,,);
    integer highs,i;
    initial begin
        @(negedge clk); reset=0;
        repeat(20) @(negedge clk);
        highs=0;
        for(i=0;i<20;i=i+1) begin if(pwm) highs=highs+1; @(negedge clk); end
        if(highs != 10) $fatal(1,"50 percent PWM");
        duty=-32768;
        wait(reversing); @(negedge clk);
        for(i=0;i<19;i=i+1) begin
            if(pwm || enabled || direction) $fatal(1,"reversal blanking");
            @(negedge clk);
        end
        repeat(2) @(negedge clk);
        if(!direction || applied != -32768) $fatal(1,"reverse application");
        #3; inhibit=1; #1;
        if(pwm || enabled) $fatal(1,"inhibit does not bypass PWM boundary");
        repeat(2) @(negedge clk); if(!direction) $fatal(1,"direction changed while inhibited");
        $display("PWM: duty, reversal blanking, immediate inhibit passed"); $finish;
    end
    initial begin #100000; $fatal(1,"timeout"); end
endmodule
