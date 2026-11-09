#!/usr/bin/env python3
"""Run original full-controller AXI/cleanup benches with real C++ PS execution."""
import argparse,subprocess,re,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--iverilog',required=True);p.add_argument('--vvp',required=True);p.add_argument('--base',default='');p.add_argument('--bench',choices=['controller_axi','controller_cleanup'],default='controller_axi');a=p.parse_args()
r=Path(__file__).resolve().parents[1];profile=r;build=a.build.resolve();out=build/('ps_'+a.bench);out.mkdir(exist_ok=True)
iv=Path(a.iverilog);vvp=Path(a.vvp);local=iv.parent.parent
subprocess.run(['g++','-std=c++14','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(local/'include/iverilog'),'-I'+str(profile/'include'),'-I'+str(r/'software/include'),str(r/'tests/ps_vpi.cpp'),str(build/'libdmotor.a'),'-o',str(out/'ps_engine.vpi')],check=True)
s=(r/f'tests/{a.bench}_tb.sv').read_text()
# One real AXI bus; serialize the original client and the test PS worker.
# Original tasks and assertions otherwise remain unchanged.
s=s.replace(' reg clk=0;', ' reg bus_lock=0,last_actor=1;reg [1:0] wants=0;\n reg clk=0;')
for task in ['write_word','read_word']:
 start=s.index(' task '+task);begin=s.index('begin',start)+5;end=s.index('end endtask',begin)
 s=s[:end]+'bus_lock=0; '+s[end:]
 s=s[:begin]+'\n  wants[actor]=1;while(bus_lock || (wants[!actor] && last_actor==actor))@(negedge clk);wants[actor]=0;bus_lock=1;last_actor=actor;\n'+s[begin:]
s=s.replace('input [1:0] expected);begin','input [1:0] expected,input bit actor=0);begin').replace('input [1:0] expected=0);begin','input [1:0] expected=0,input bit actor=0);begin')
# Distinct task invocations may overlap while waiting for bus ownership.
s=s.replace('task write_word','task automatic write_word').replace('task read_word','task automatic read_word')
# Cold-reset scenarios arbitrate with both bus masters; avoid abandoning a test
# master halfway through a transfer and deadlocking the stimulus itself.
s=s.replace('@(negedge clk);reset=1;', '@(negedge clk);while(bus_lock)@(negedge clk);bus_lock=1;reset=1;')
s=s.replace('@(negedge clk);reset=0;', '@(negedge clk);reset=0;bus_lock=0;')

worker='''
 reg [1119:0] ps_record;reg [511:0] ps_reply;
 reg [63:0] ps_ticket;integer ps_i;integer ps_transactions=0;
 initial begin
  wait(!reset);
  forever begin
   repeat(4)@(negedge clk);
   if(!reset)begin
    read_word('h4024);if(value==0)write_word('h4024,1,0);
    read_word('h4008);
    if(value[0]&&!value[1])begin
     read_word('h4010);ps_ticket[31:0]=value;
     read_word('h4014);ps_ticket[63:32]=value;
     for(ps_i=0;ps_i<35;ps_i=ps_i+1)begin
      read_word('h4080+4*ps_i);ps_record[ps_i*32+:32]=value;
     end
     $ps_execute(ps_record,ps_reply);
     write_word('h4018,ps_ticket[31:0],0);write_word('h401c,ps_ticket[63:32],0);
     for(ps_i=0;ps_i<16;ps_i=ps_i+1)write_word('h4140+4*ps_i,ps_reply[ps_i*32+:32],0);
     write_word('h4020,1,0);ps_transactions=ps_transactions+1;
    end
   end
  end
 end
'''
worker=re.sub(r'read_word\(([^;]+)\);',r'read_word(\1,0,1);',worker)
worker=re.sub(r'write_word\(([^;]+)\);',r'write_word(\1,1);',worker)
s=s.replace('endmodule',worker+'\nendmodule')
s=s.replace('both HLS kernels','atomic HLS + actual C++ PS engine')
# Main benches deliberately reset hardware repeatedly. Discard a captured request
# if reset/cancellation intervened; the subsequent PL cleanup resets all contexts.
s=s.replace('     $ps_execute(ps_record,ps_reply);', '''     read_word('h4008,0,1);
     if(value[0])begin
     $ps_execute(ps_record,ps_reply);''').replace("write_word('h4020,1,0,1);ps_transactions=ps_transactions+1;","write_word('h4020,1,0,1);ps_transactions=ps_transactions+1;end")
(out/f'{a.bench}_tb.sv').write_text(s)
files=['timing','controller_registers','batch_registers','atomic_hls_adapter','atomic_mailbox_arbiter','shared_scheduler','target_age','pwm','shared_control','mailbox_control','shared_engine_transport','motor_engine_adapter','cleanup_sequencer','integrated_control','safety','qcencoder','encoder_snapshot','motor_io','monitor_heartbeat','board_status','axi_host','controller_axi']
atomic=r/'build/vitis_atomic/hls/syn/verilog'
cmd=[str(iv),'-g2012','-s',a.bench+'_tb','-o',str(out/(a.bench+'.vvp'))]+[str(profile/f'rtl/{n}.sv') for n in files]+[str(x) for x in sorted(atomic.glob('*.v'))]+[str(out/f'{a.bench}_tb.sv')]
if a.base:cmd[1:1]=['-B',a.base]
subprocess.run(cmd,check=True)
run=subprocess.run([str(vvp),'-M',str(out),'-m','ps_engine',str(out/(a.bench+'.vvp'))],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,cwd=atomic,timeout=1200)
(out/(a.bench+'.txt')).write_text(run.stdout);print(run.stdout);run.check_returncode()
assert a.bench+' PASS:' in run.stdout
