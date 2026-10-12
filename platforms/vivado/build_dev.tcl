# Run from any directory: vivado -mode batch -source platforms/vivado/build_dev.tcl
set root [file normalize [file join [file dirname [info script]] ../..]]
set out [file join $root build vivado_dev]
file mkdir $out
create_project -force dmotor_dev [file join $out project] -part xc7z020clg400-1
add_files [file join $root rtl vivado_dev.sv]
set_property top dmotor_vivado_dev [current_fileset]
ipx::package_project -root_dir [file join $out ip] -vendor dmotor.org -library user -taxonomy /UserIP -import_files -set_current true
set core [ipx::current_core]
set_property name dmotor_dev $core
set_property display_name {DMOTOR inhibited development endpoint} $core
set_property description {AXI diagnostics only. Motor drive permanently inhibited. No motor engine.} $core
set_property version 1.0 $core
ipx::infer_bus_interfaces xilinx.com:interface:aximm_rtl:1.0 $core
ipx::infer_bus_interfaces xilinx.com:signal:clock_rtl:1.0 $core
ipx::infer_bus_interfaces xilinx.com:signal:reset_rtl:1.0 $core
set bus [ipx::get_bus_interfaces -of_objects $core -filter {BUS_TYPE_NAME == aximm}]
if {[llength $bus] != 1} {error "Expected one inferred AXI interface"}
set_property name S_AXI $bus
set busname S_AXI
foreach old_map [ipx::get_memory_maps -of_objects $core] {ipx::remove_memory_map [get_property NAME $old_map] $core}
ipx::associate_bus_interfaces -busif $busname -clock aclk $core
set mmap [ipx::add_memory_map registers $core]
set block [ipx::add_address_block registers $mmap]
set_property range 65536 $block
set_property width 32 $block
set_property usage register $block
set_property slave_memory_map_ref registers $bus
ipx::create_xgui_files $core
ipx::update_checksums $core
ipx::check_integrity $core
ipx::save_core $core
close_project
create_project -in_memory -part xc7z020clg400-1
read_verilog -sv [file join $root rtl vivado_dev.sv]
synth_design -top dmotor_vivado_dev -mode out_of_context -part xc7z020clg400-1
create_clock -name fabric -period 50 [get_ports aclk]
set_input_delay 5 -clock fabric [get_ports -filter {DIRECTION == IN && NAME != aclk}]
set_output_delay 5 -clock fabric [get_ports -filter {DIRECTION == OUT}]
opt_design
place_design
route_design
report_utilization -file [file join $out utilization.rpt]
report_timing_summary -delay_type min_max -file [file join $out timing.rpt]
report_drc -file [file join $out drc.rpt]
write_checkpoint -force [file join $out routed.dcp]
if {[get_property SLACK [get_timing_paths -delay_type max -max_paths 1]] < 0} {error "Setup timing failed"}
if {[get_property SLACK [get_timing_paths -delay_type min -max_paths 1]] < 0} {error "Hold timing failed"}
puts "DMOTOR development IP packaged and OOC routed; not a board bitstream."
