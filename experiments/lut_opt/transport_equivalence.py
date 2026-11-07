#!/usr/bin/env python3
"""Compare transport cycles with adversarial dual-port memory traffic.

HLS shells are deliberately inert; forced ports exercise every read selector,
simultaneous writes/collisions, completion, timeout and response backpressure.
Actual generated HLS behavior is covered separately by the AXI/cleanup benches.
"""
import argparse
from pathlib import Path
import re
import subprocess
p=argparse.ArgumentParser()
p.add_argument('--iverilog',required=True)
p.add_argument('--vvp',required=True)
p.add_argument('--base',default='')
a=p.parse_args()
r=Path(__file__).resolve().parents[2]
out=r/'build/opt/transport_equivalence';out.mkdir(parents=True,exist_ok=True)
for name,core,generated,specs,inputs in [
 ('shared_engine_transport','dmotor_mailbox_shared',r/'build/product/engine/build/vitis_dispatcher/hls/syn/verilog',
  [('request',2),('configuration',5)], [('command_record',1120)]),
 ('atomic_hls_adapter','dmotor_atomic_mailbox',r/'build/vitis_atomic/hls/syn/verilog',
  [('request',3),('admission',4)], [('command_words',160),('admission_words',352),('command_tick',64)])]:
 source=subprocess.check_output(['git','show','67cb5da:rtl/'+name+'.sv'],cwd=r,text=True)
 ref=out/(name+'_reference.sv');ref.write_text(source.replace('module dmotor_'+name,'module reference_'+name))
 text=(generated/(core+'.v')).read_text()
 header=re.search(r'module '+core+r'\s*\(.*?\);',text,re.S).group()
 declarations='\n'.join(re.findall(r'^(?:input|output|inout)\s+[^;]*;',text,re.M))
 stub=out/(core+'.v');stub.write_text(header+'\n'+declarations+'\nendmodule\n')
 s='''`timescale 1ns/1ps
module transport_tb;
 reg clk=0;always #5 clk=~clk;
 reg reset=1,command_valid=0,response_ready=0,done=0;
 reg [31:0] index0=0,index1=0,data0=0,data1=0;
 reg ce0=0,ce1=0;
 wire [579:0] actual,expected;
'''
 for port,bits in inputs:s+=f' reg [{bits-1}:0] {port}=0;\n'
 for bits in {4, *(bits for _,bits in specs)}:
  for port in [0,1]:s+=f' wire [{bits-1}:0] index{port}_{bits}=index{port}[{bits-1}:0];\n'
 for instance,prefix,bundle in [('dut','dmotor','actual'),('refdut','reference','expected')]:
  s+=f' {prefix}_{name} #(.MAX_CYCLES(2048)) {instance}(.clk(clk),.reset(reset),.command_valid(command_valid),.response_ready(response_ready),\n'
  s+=','.join(f'.{port}({port})' for port,_ in inputs)+',\n'
  s+=f'''.command_ready({bundle}[0]),.response_valid({bundle}[1]),.response_error({bundle}[2]),.fault({bundle}[3]),
 .response_words({bundle}[515:4]),.last_latency({bundle}[547:516]),.maximum_latency({bundle}[579:548]));
'''
 s+=' integer i,j;\n initial begin\n'
 for inst in ['dut','refdut']:
  s+=f' force {inst}.hls_done=done;force {inst}.hls_ready=1;\n'
  for label,bits in specs:
   for port in [0,1]:s+=f' force {inst}.{label}_address{port}=index{port}_{bits};force {inst}.{label}_ce{port}=ce{port};\n'
  for port in [0,1]:
   s+=f' force {inst}.reply_address{port}=index{port}_4;force {inst}.reply_ce{port}=ce{port};force {inst}.reply_we{port}=1;force {inst}.reply_d{port}=data{port};\n'
 s+=' repeat(3) @(posedge clk);\n for(i=0;i<5000;i=i+1)begin\n @(negedge clk);#1;\n'
 s+=' reset=(i==1600);command_valid=(i==1 || i==1602);response_ready=(i%7==0);done=(i==1500);\n'
 for port,bits in inputs:
  s+=f' for(j=0;j<{bits//32};j=j+1){port}[j*32 +:32]=32\'h9abcdef0^(j*7919)^i;\n'
 s+=' index0=i%32;index1=(i%5==0 ? index0 : (i/32)%32);data0=i*12345;data1=~data0;ce0=(i%3!=0);ce1=(i%4!=0);\n end\n'
 s+=f' @(negedge clk);$display("TRANSPORT_EQUIVALENCE_PASS: {name}, 5000 cycles");$finish;end\n'
 s+=' always @(negedge clk)begin\n if(actual !== expected)$fatal(1,"transport output mismatch");\n'
 for label,_ in specs:
  for port in [0,1]:s+=f' if(dut.{label}_q{port} !== refdut.{label}_q{port})$fatal(1,"{label} port {port} mismatch");\n'
 s+=' end\nendmodule\n'
 bench=out/(name+'_tb.sv');bench.write_text(s)
 cmd=[a.iverilog,'-g2012','-s','transport_tb','-o',str(out/(name+'.vvp'))]
 if a.base:cmd+=['-B',a.base]
 subprocess.run(cmd+list(map(str,[r/'rtl/timing.sv',stub,ref,r/'rtl'/(name+'.sv'),bench])),check=True)
 subprocess.run([a.vvp,str(out/(name+'.vvp'))],check=True)
