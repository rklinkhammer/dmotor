#!/usr/bin/env python3
"""Prepare an opt-in parallel PS profile; never modify product sources or current IP."""
import argparse,json,shutil,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--period-us',type=int,choices=[1000,5000],default=1000)
a=p.parse_args();root=Path(__file__).resolve().parents[2];out=a.output.resolve();here=Path(__file__).resolve().parent
if out.exists():p.error('Use a new output directory')
if root in out.parents and root/'build' not in out.parents:
 p.error('Place generated profiles under build/ or outside the source repository')
subprocess.run([sys.executable,str(root/'tools/prepare_shared_mailbox.py'),'--output',str(out)],check=True)
def edit(path,old,new):
 s=path.read_text()
 if s.count(old)!=1:raise RuntimeError(f'Review changed insertion point: {path}: {old}')
 path.write_text(s.replace(old,new))
if a.period_us==5000:
 path=out/'abi/timing.json';v=json.loads(path.read_text());v['control_hz']=200;v['control_deadline_us']=4800;path.write_text(json.dumps(v,indent=2)+'\n')
 edit(out/'tools/generate_abi.py',"assert timing['control_hz'] == 1000","assert timing['control_hz'] == 200 # PS profile: reviewed 9-bit event bound and 37-bit integral")
 subprocess.run([sys.executable,str(out/'tools/generate_abi.py')],check=True)
 edit(out/'include/dmotor/operating_limits.hpp','static_assert(timing::control_hz == 1000, "Re-derive integral width for changed control rate");','static_assert(timing::control_hz == 200, "PS 5 ms profile"); // 37 bits also cover the smaller integral scale.')
 # 500 events/tick (9 bits), 5e6 scaled events, numerator <=6e18; uint64_t fits.
 edit(out/'src/velocity.cpp','static_cast<uint32_t>(edges) & 0x7fU','static_cast<uint32_t>(edges) & 0x1ffU')
 # External settle_ticks remains milliseconds. Round observation up to the next
 # 5 ms boundary, never shorten the configured settling duration.
 edit(out/'src/motion.cpp','(settling_ + 1) & 0xfffffU','settling_ + 5')
 from adapt_rate_tests import adapt
 adapt(out)
subprocess.run([sys.executable,str(out/'tools/generate_mailbox_abi.py')],check=True)
ports='''
    input wire ps_power_reset,ps_valid,ps_write,
    input wire [15:0] ps_address,input wire [31:0] ps_wdata,input wire [3:0] ps_strobes,
    output wire ps_ready,ps_response_valid,input wire ps_response_ready,
    output wire ps_response_okay,output wire [31:0] ps_rdata,
'''
names=['ps_power_reset','ps_valid','ps_write','ps_address','ps_wdata','ps_strobes','ps_ready','ps_response_valid','ps_response_ready','ps_response_okay','ps_rdata']
connections=','.join('.'+n+'('+n+')' for n in names)+','
for name,child in [('motor_io','dmotor_integrated_control'),('integrated_control','dmotor_motor_engine_adapter'),('motor_engine_adapter','dmotor_shared_engine_transport')]:
 path=out/f'rtl/{name}.sv';edit(path,'    input wire clk, reset,',ports+'    input wire clk, reset,')
 s=path.read_text();start=s.index(child+' ');instance={'motor_io':'controller','integrated_control':'engine','motor_engine_adapter':'transport'}[name];inst=s.index(' '+instance+'(',start)+len(instance)+1;s=s[:inst+1]+connections+s[inst+1:];path.write_text(s)
shutil.copyfile(here/'shared_engine_transport.sv',out/'rtl/shared_engine_transport.sv')
path=out/'rtl/motor_engine_adapter.sv'
edit(path,'(apply_target ? STAGE_SESSION : TICK)','TICK')
edit(path,'end else if(operation==COMMIT_TARGET)record[11*32 +:32]=2;',"""end else if(operation==COMMIT_TARGET)record[11*32 +:32]=2;
        if(operation==TICK && saved_apply)begin
            record[11*32 +:32]=3;record[12*32 +:32]=selected_sequence;
            record[13*32 +:32]=saved_target;
        end""")

path=out/'rtl/controller_axi.sv'
edit(path,'dmotor_motor_io controller(', 'dmotor_motor_io controller('+connections)
router='''wire ps_power_reset=reset;
 wire ps_valid,ps_write,ps_ready,ps_response_valid,ps_response_ready,ps_response_okay;
 wire [15:0] ps_address;wire [31:0] ps_wdata,ps_rdata;wire [3:0] ps_strobes;
 wire bus_valid,bus_write,bus_ready,bus_response_valid,bus_response_ready,bus_response_okay;
 wire [31:0] bus_address,bus_wdata,bus_rdata;wire [3:0] bus_strobes;
 reg route_busy,route_ps;
 wire select_ps=bus_address>=32'h4000 && bus_address<32'h4200;
 assign host_valid=bus_valid&&!route_busy&&!select_ps;
 assign host_write=bus_write;assign host_address=bus_address;
 assign host_wdata=bus_wdata;assign host_strobes=bus_strobes;
 assign ps_valid=bus_valid&&!route_busy&&select_ps;
 assign ps_write=bus_write;assign ps_address=bus_address[15:0];
 assign ps_wdata=bus_wdata;assign ps_strobes=bus_strobes;
 assign bus_ready=!route_busy && (select_ps?ps_ready:host_ready);
 assign bus_response_valid=route_busy && (route_ps?ps_response_valid:host_response_valid);
 assign bus_response_okay=route_ps?ps_response_okay:host_response_okay;
 assign bus_rdata=route_ps?ps_rdata:host_rdata;
 assign host_response_ready=route_busy&&!route_ps&&bus_response_ready;
 assign ps_response_ready=route_busy&&route_ps&&bus_response_ready;
 always @(posedge clk)begin
  if(reset)begin route_busy<=0;route_ps<=0;end
  else begin
   if(bus_valid&&bus_ready)begin route_busy<=1;route_ps<=select_ps;end
   if(bus_response_valid&&bus_response_ready)route_busy<=0;
  end
 end
 dmotor_axi_host bus(.host_valid(bus_valid),.host_write(bus_write),.host_ready(bus_ready),
 .host_address(bus_address),.host_wdata(bus_wdata),.host_strobes(bus_strobes),
 .host_response_valid(bus_response_valid),.host_response_ready(bus_response_ready),
 .host_response_okay(bus_response_okay),.host_rdata(bus_rdata),.*);'''
edit(path,'dmotor_axi_host bus(.*);',router)
shutil.copytree(root/'constraints',out/'constraints')
# Software lives in the generated profile, keeping the default daemon/driver intact.
shutil.copytree(root/'software',out/'software')
(out/'ps_parallel').mkdir()
for name in ['engine.hpp','ps_device.hpp','ps_device.cpp']:
 shutil.copyfile(here/name,out/'ps_parallel'/name)
edit(out/'software/apps/daemon.cpp','#include "dmotor_sw/kernel_device.hpp"','#ifdef DMOTOR_TEST_SERVICE\n#include "dmotor_sw/kernel_device.hpp"\n#else\n#include "ps_device.hpp"\n#endif')
edit(out/'software/apps/daemon.cpp','KernelDevice device(argv[2]);','#ifdef DMOTOR_TEST_SERVICE\n        KernelDevice device(argv[2]);\n#else\n        dmotor_ps::PsDevice device(argv[2]);\n#endif')
# Retain service protocol, client library and commissioning-only release policy.
with (out/'software/CMakeLists.txt').open('a') as f:
 f.write('''
# Explicit PS parallel backend. No default product target references this tree.
find_package(Threads REQUIRED)
add_library(dmotor_ps_engine STATIC
 ../src/protocol.cpp ../src/arithmetic.cpp ../src/control.cpp ../src/command_engine.cpp
 ../src/motion.cpp ../src/velocity.cpp ../src/controller.cpp ../src/mailbox.cpp)
target_include_directories(dmotor_ps_engine PUBLIC ../include ../ps_parallel)
target_compile_features(dmotor_ps_engine PUBLIC cxx_std_14)
target_compile_options(dmotor_ps_engine PRIVATE -Wall -Wextra -Werror)
target_sources(dmotord PRIVATE ../ps_parallel/ps_device.cpp)
target_link_libraries(dmotord PRIVATE dmotor_ps_engine Threads::Threads)
''')
path=out/'software/linux/driver/dmotor_access.c'
edit(path,'return a <= 0x40 ||','return a == 0x4000 || a == 0x4004 || a == 0x4008 || a == 0x4010 || a == 0x4014 ||\n        a == 0x4024 || a == 0x4028 || a == 0x402c || (a >= 0x4080 && a < 0x410c) ||\n        a <= 0x40 ||')
edit(path,' if ((a >= 0x10', ' if (a == 0x4018 || a == 0x401c || (a >= 0x4140 && a < 0x4180)) return true;\n if (a == 0x4020 || a == 0x4024) return v == 1;\n if ((a >= 0x10')
edit(path,'resource_size(r) < (DM_TELEMETRY_BASE + 4 * DM_TELEMETRY_STRIDE)','resource_size(r) < 0x4180')
edit(path,' ret = halt_all(d); if (ret) goto fail;',' if (readl(d->base + 0x4000) != 0x50534631U) { ret = -ENODEV; goto fail; }\n ret = halt_all(d); if (ret) goto fail;')
(out/'ps_profile.json').write_text(json.dumps({'profile':'PSF1','period_us':a.period_us,'source':str(root),'baseline':'400 RPM exact-onephase velocity product','hardware_ports':'unchanged','extension':'0x4000..0x417c','production_default':False},indent=2)+'\n')
from provenance import configure
configure(out,a.period_us)
print('Prepared parallel profile:',out)
