#!/usr/bin/env python3
"""Cycle comparison against the immutable pre-optimization Git baseline."""
import argparse
from pathlib import Path
import subprocess
p = argparse.ArgumentParser()
p.add_argument('--iverilog', required=True)
p.add_argument('--vvp', required=True)
p.add_argument('--base', default='')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = root / 'build/opt/equivalence'
out.mkdir(parents=True, exist_ok=True)
files = []
for name in ('pwm', 'monitor_heartbeat'):
    source = subprocess.check_output(['git', 'show', '67cb5da:rtl/' + name + '.sv'], cwd=root, text=True)
    target = out / (name + '_reference.sv')
    target.write_text(source.replace('module dmotor_' + name, 'module reference_' + name))
    files += [target, root / 'rtl' / (name + '.sv')]
bench = r'''
`timescale 1ns/1ps
module pwm_pair #(parameter PERIOD=1000)(input clk,reset,inhibit,input signed [31:0] duty);
 wire [132:0] actual, expected;
 dmotor_pwm #(.PERIOD_CYCLES(PERIOD)) candidate(.clk(clk),.reset(reset),.inhibit(inhibit),.requested_duty_q16(duty),
 .pwm(actual[0]),.drive_enable(actual[1]),.direction(actual[2]),.reversing(actual[3]),
 .applied_duty_q16(actual[35:4]),.timestamp(actual[99:36]),.application_timestamp(actual[163:100]));
 reference_pwm #(.PERIOD_CYCLES(PERIOD)) reference(.clk(clk),.reset(reset),.inhibit(inhibit),.requested_duty_q16(duty),
 .pwm(expected[0]),.drive_enable(expected[1]),.direction(expected[2]),.reversing(expected[3]),
 .applied_duty_q16(expected[35:4]),.timestamp(expected[99:36]),.application_timestamp(expected[163:100]));
 always @(negedge clk) if(actual !== expected)$fatal(1,"PWM mismatch period=%0d",PERIOD);
endmodule
module heartbeat_pair #(parameter STARTUP=32,TIMEOUT=16,QUALIFY=64,MIN_EDGE=2)(input clk,reset,core_reset,hb_async,acknowledge);
 wire [2:0] actual,expected;
 dmotor_monitor_heartbeat #(.STARTUP_CYCLES(STARTUP),.TIMEOUT_CYCLES(TIMEOUT),.QUALIFY_CYCLES(QUALIFY),.MIN_EDGE_CYCLES(MIN_EDGE)) candidate
 (.clk(clk),.reset(reset),.core_reset(core_reset),.hb_async(hb_async),.acknowledge(acknowledge),.fault(actual[0]),.healthy(actual[1]),.qualified(actual[2]));
 reference_monitor_heartbeat #(.STARTUP_CYCLES(STARTUP),.TIMEOUT_CYCLES(TIMEOUT),.QUALIFY_CYCLES(QUALIFY),.MIN_EDGE_CYCLES(MIN_EDGE)) reference
 (.clk(clk),.reset(reset),.core_reset(core_reset),.hb_async(hb_async),.acknowledge(acknowledge),.fault(expected[0]),.healthy(expected[1]),.qualified(expected[2]));
 always @(negedge clk) if(actual !== expected)$fatal(1,"Heartbeat mismatch startup=%0d",STARTUP);
endmodule
module equivalence_tb;
 reg clk=0;always #5 clk=~clk;
 reg reset=1,inhibit=0,core_reset=1,hb_async=0,acknowledge=0;
 reg signed [31:0] duty=0;
 reg [31:0] rng=32'h12345678;
 pwm_pair #(1) p1(.*);pwm_pair #(2) p2(.*);pwm_pair #(3) p3(.*);
 pwm_pair #(1000) p1000(.*);pwm_pair #(1024) p1024(.*);
 heartbeat_pair h(.*);heartbeat_pair #(1,1,1,1) h1(.*);
 heartbeat_pair #(7,13,9,2) hn(.*);
 integer cycle;
 initial begin
  repeat(3) @(posedge clk);
  for(cycle=0;cycle<150000;cycle=cycle+1)begin
   @(negedge clk);#1;
   rng=rng^(rng<<13);rng=rng^(rng>>17);rng=rng^(rng<<5);
   reset=(cycle%20000==0);inhibit=(cycle%5011<3);
   core_reset=rng[0];acknowledge=rng[1];
   if(cycle%7==0 && cycle%1000<850)hb_async=~hb_async;
   if(cycle%1300==0)begin
    case((cycle/1300)%8)
     0:duty=32'h80000000;1:duty=32'h7fffffff;2:duty=65536;
     3:duty=-65536;4:duty=65535;5:duty=-1;6:duty=0;7:duty=$signed(rng);
    endcase
   end
  end
  @(negedge clk);$display("EQUIVALENCE_PASS: 150000 cycles, 5 PWM periods, 3 heartbeat profiles");$finish;
 end
endmodule
'''.replace('[132:0]', '[163:0]')
(out / 'bench.sv').write_text(bench)
cmd = [a.iverilog, '-g2012', '-s', 'equivalence_tb', '-o', str(out/'test.vvp')]
if a.base: cmd += ['-B', a.base]
subprocess.run(cmd + [str(root/'rtl/timing.sv')] + list(map(str, files)) + [str(out/'bench.sv')], check=True)
subprocess.run([a.vvp, str(out/'test.vvp')], check=True)
