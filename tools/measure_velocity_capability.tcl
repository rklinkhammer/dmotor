# Compare complete controller implementation with a selected generated motor core.
# Usage: vivado -mode batch -source tools/measure_velocity_capability.tcl \
#   -tclargs <prepared-profile> <output-directory>
if {[llength $argv] != 2} {error "Expected prepared-profile output-directory"}
lassign $argv profile out
set root [file normalize [file join [file dirname [info script]] ..]]
set profile [file normalize $profile]
set out [file normalize $out]
file mkdir $out
cd $out
create_project -force capability [file join $out project] -part xc7z020clg400-1
foreach f {timing controller_registers batch_registers atomic_hls_adapter atomic_mailbox_arbiter shared_scheduler target_age pwm shared_control mailbox_control shared_engine_transport motor_engine_adapter integrated_control safety qcencoder encoder_snapshot motor_io axi_host controller_axi} {
    add_files [file join $root rtl ${f}.sv]
}
foreach dir [list [file join $root build vitis_atomic hls syn verilog] [file join $profile build vitis_dispatcher hls syn verilog]] {
    add_files [glob [file join $dir *.v]]
    foreach pat {*.dat *.mem} {foreach f [glob -nocomplain [file join $dir $pat]] {add_files $f}}
}
set_property top dmotor_controller_axi [current_fileset]
update_compile_order -fileset sources_1
synth_design -top dmotor_controller_axi -part xc7z020clg400-1 -mode out_of_context
create_clock -name controller_clk -period 50 [get_ports clk]
source [file join $root constraints controller_halt.xdc]
report_utilization -file [file join $out synthesized_utilization.rpt]
opt_design
place_design
route_design
report_utilization -file [file join $out routed_utilization.rpt]
report_timing_summary -file [file join $out timing.rpt]
report_drc -file [file join $out drc.rpt]
write_checkpoint -force [file join $out routed.dcp]
foreach delay {max min} {
    set paths [get_timing_paths -delay_type $delay -max_paths 1]
    if {![llength $paths] || [get_property SLACK $paths] < 0} {error "$delay timing failed"}
}
if {[llength [get_drc_violations -filter {SEVERITY == Error}]]} {error "DRC errors"}
puts "CAPABILITY_ROUTED_PASS"
close_project
