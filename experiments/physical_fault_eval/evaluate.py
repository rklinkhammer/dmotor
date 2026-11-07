#!/usr/bin/env python3
"""Generate an isolated four-channel protection-cone equivalence experiment."""
from pathlib import Path
import argparse, subprocess, json
r=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=r/'build/physical_fault_eval');p.add_argument('--seed',type=int,default=0x197403);a=p.parse_args();o=a.out.resolve();o.mkdir(parents=True,exist_ok=True)
s=(r/'rtl/safety.sv').read_text();(o/'baseline_safety.sv').write_text(s)
c=s.split('\nmodule dmotor_halt_aggregator')[0].replace('module dmotor_safety #','module candidate_safety #')
c=c.replace('output reg [31:0] faults,','output wire [31:0] faults,\n    input wire physical_latched,\n    output wire any_new_fault,')
c=c.replace('    wire halt_sync;','    reg [31:0] local_latched;\n    assign faults = local_latched | (physical_latched ? 32\'h4 : 0);\n    assign any_new_fault = |new_faults;\n    wire halt_sync;')
c=c.replace('faults <= 0;', 'local_latched <= 0;').replace('faults <= faults & ~(acknowledge_mask & ~local_faults);','local_latched <= faults & ~(acknowledge_mask & ~local_faults) & ~32\'h4;').replace('faults <= faults | new_faults;', 'local_latched <= (faults | new_faults) & ~32\'h4;')
(o/'candidate_safety.sv').write_text(c)
ports='''input wire clk,reset,core_reset,halt_async,inhibit,ctrl_fault,ctrl_halt,ack_local,ack_external,ack_halt,
input wire [3:0] physical,enable_request,disable_request,authorized,refresh,
input wire [127:0] other_faults,sequences,
input wire [31:0] session,refresh_session,
input wire [63:0] timeout,
output wire [3:0] enabled,external_latched,
output wire [127:0] faults,expected,
output wire [255:0] fault_times,last_refresh,
output wire global_inhibit,shared_halt,coast_req'''
body='''
(* ASYNC_REG="TRUE" *) reg [3:0] physical_meta,physical_sync;
(* ASYNC_REG="TRUE", DONT_TOUCH="TRUE" *) reg halt_meta,halt_sync;
always @(posedge clk) begin
 if(reset) begin physical_meta<=0;physical_sync<=0;halt_meta<=0;halt_sync<=0;end
 else begin physical_meta<=physical;physical_sync<=physical_meta;halt_meta<=halt_async;halt_sync<=halt_meta;end
end
wire [3:0] requests,safety_inhibits,halt_detected;
wire aggregated;
assign global_inhibit=inhibit || aggregated || |halt_detected || |external_latched || |physical_sync || ctrl_fault || |(enabled & safety_inhibits);
assign shared_halt=aggregated || |requests || ctrl_halt;
assign coast_req=shared_halt || halt_sync || |external_latched;
dmotor_halt_aggregator #(.MOTORS(5)) agg(.clk(clk),.reset(reset),.acknowledge(ack_halt),.requests({ctrl_halt,requests}),.halted(aggregated));
CENTRAL
for(genvar m=0;m<4;m=m+1) begin: channel
 SAFETY #(.HALT_ALREADY_SYNCHRONIZED(1)) s(.clk(clk),.reset(reset),.halt_async(halt_sync),
 .local_faults((other_faults[m*32+:32] & 32'h18) | (physical_sync[m] ? 32'h4 : 0) | (ctrl_fault ? 32'h2 : 0)),
 .enable_request(enable_request[m]),.disable_request(disable_request[m] || global_inhibit),
 .prerequisites_valid(authorized[m] && !global_inhibit),
 .acknowledge_local(ack_local && !enabled[m]),.acknowledge_external(ack_external && !enabled[m]),
 .timeout_cycles(timeout),.refresh_valid(refresh[m]),.session(session),.refresh_session(refresh_session),
 .refresh_sequence(sequences[m*32+:32]),.enabled(enabled[m]),.inhibit(safety_inhibits[m]),
 .halt_out(requests[m]),.external_latched(external_latched[m]),.faults(faults[m*32+:32]),
 .expected_sequence(expected[m*32+:32]),.timestamp(),.last_refresh(last_refresh[m*64+:64]),
 .first_fault_timestamp(fault_times[m*64+:64]),.halt_detected(halt_detected[m]),.acknowledge_mask(32'hffffffff) EXTRA);
end
endmodule
'''
central='''reg [3:0] physical_latched;
wire [3:0] any_new_fault;
for(genvar j=0;j<4;j=j+1) begin: system_source
 always @(posedge clk) begin
  if(reset) physical_latched[j]<=0;
  else if(physical_sync[j]) physical_latched[j]<=1;
  else if(ack_local && !enabled[j] GUARD) physical_latched[j]<=0;
 end
end'''
for name in ['baseline','candidate','simplified']:
 b=body.replace('SAFETY','dmotor_safety' if name=='baseline' else 'candidate_safety')
 b=b.replace('CENTRAL','' if name=='baseline' else central.replace('GUARD','&& !any_new_fault[j]' if name=='candidate' else ''))
 b=b.replace('EXTRA','' if name=='baseline' else ',.physical_latched(physical_latched[m]),.any_new_fault(any_new_fault[m])')
 (o/(name+'.sv')).write_text('module '+name+'('+ports+');\n'+b)
tb='''`timescale 1ns/1ps
module tb;
reg clk=0;always #5 clk=~clk;
reg reset=1,core_reset=0,halt_async=0,inhibit=0,ctrl_fault=0,ctrl_halt=0,ack_local=0,ack_external=0,ack_halt=0;
reg [3:0] physical=0,enable_request=0,disable_request=0,authorized=15,refresh=0;
reg [127:0] other_faults=0,sequences=0;
reg [31:0] session=9,refresh_session=9;
reg [63:0] timeout=20;
OUTPUTS
INSTANCES
integer cycles=0,seed=32'h197403,randval,j,k,negative_mismatches=0;
task step;begin
 @(posedge clk);#1;cycles=cycles+1;
 if(a !== b) begin $display("cycle=%0d physical=%h other=%h ack=%b a=%h b=%h",cycles,physical,other_faults,ack_local,a,b);$fatal(1,"candidate mismatch");end
 if(a !== c) negative_mismatches=negative_mismatches+1;
 @(negedge clk);
end endtask
initial begin
 step();reset=0;enable_request=15;step();enable_request=0;
 // Physical fault followed by a concurrent different fault and acknowledgement.
 physical=1;repeat(4) step();physical=0;repeat(3) step();
 other_faults=8;ack_local=1;step();
 if(negative_mismatches==0) $fatal(1,"simplified clear rule was not rejected");
 other_faults=0;repeat(3) step();ack_halt=1;repeat(3) step();
 ack_external=1;repeat(3) step();
 if(a_enabled!=0) $fatal(1,"automatic enable after clear");
 // All physical masks, release, core-reset hold, and explicit acknowledgement.
 for(k=1;k<16;k=k+1) begin
 reset=1;step();reset=0;ack_local=0;ack_halt=0;ack_external=0;enable_request=15;step();enable_request=0;
 physical=k;repeat(4) step();physical=0;repeat(3) step();core_reset=1;repeat(3) step();core_reset=0;
 if(!a_shared_halt || !a_coast_req || a_enabled!=0) $fatal(1,"protection did not persist");
 ack_local=1;repeat(2) step();ack_halt=1;repeat(2) step();
 end
 // Deterministic randomized differential stimulus, biased toward recovery/re-enable.
 for(k=0;k<30000;k=k+1) begin
 randval=$random(seed);reset=(k%101==0);core_reset=randval[0];
 physical=(k%23<3) ? randval[7:4] : 0;
 other_faults=0;
 for(j=0;j<4;j=j+1) begin
 if(k%29<4) other_faults[j*32+:32]=32'b1 << ((j+k)%5);
 sequences[j*32+:32]=a_expected[j*32+:32];
 end
 halt_async=(k%41==1);inhibit=(k%53==1);ctrl_halt=(k%61==1);ctrl_fault=(k%37==1);
 ack_local=randval[8];ack_external=randval[9];ack_halt=randval[10];
 enable_request=randval[14:11];disable_request=(k%17==0)?randval[18:15]:0;
 refresh=randval[22:19];authorized=randval[26:23];refresh_session=(k%7==0)?8:9;
 timeout=(k%31)+1;step();
 end
 $display("PASS: %0d cycles exact outputs; all physical masks; reset/recovery/watchdog/overlap; simplified rule rejected (%0d mismatching cycles)",cycles,negative_mismatches);$finish;
end
endmodule
'''
outs=[('enabled',4),('external_latched',4),('faults',128),('expected',128),('fault_times',256),('last_refresh',256),('global_inhibit',1),('shared_halt',1),('coast_req',1)]
inputs=['clk','reset','core_reset','halt_async','inhibit','ctrl_fault','ctrl_halt','ack_local','ack_external','ack_halt','physical','enable_request','disable_request','authorized','refresh','other_faults','sequences','session','refresh_session','timeout']
outputtext='';inst=''
for tag,name in zip('abc',['baseline','candidate','simplified']):
 for n,w in outs:outputtext+=f'wire [{w-1}:0] {tag}_{n};\n'
 outputtext+=f'wire [{sum(w for _,w in outs)-1}:0] {tag} = '+'{'+','.join(tag+'_'+n for n,_ in outs)+'};\n'
 inst+=name+' '+tag+'_dut('+','.join('.'+n+'('+n+')' for n in inputs)+','+','.join('.'+n+'('+tag+'_'+n+')' for n,_ in outs)+');\n'
(o/'tb.sv').write_text(tb.replace('OUTPUTS',outputtext).replace('INSTANCES',inst).replace("32'h197403", str(a.seed)))
iver=r/'build/tools/usr/bin/iverilog';vvp=r/'build/tools/usr/bin/vvp';base=r/'build/tools/usr/lib/x86_64-linux-gnu/ivl'
cmd=[str(iver),'-B',str(base),'-g2012','-s','tb','-o',str(o/'sim')]+[str(o/f) for f in ['baseline_safety.sv','candidate_safety.sv','baseline.sv','candidate.sv','simplified.sv','tb.sv']]
subprocess.run(cmd,check=True);result=subprocess.run([str(vvp),str(o/'sim')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(o/'simulation.log').write_text(result.stdout);print(result.stdout);result.check_returncode()
