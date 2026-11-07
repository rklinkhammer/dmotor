# Isolated complete-controller measurement; no IP packaging or hardware access.
# Arguments: RTL directory, atomic RTL directory, engine RTL directory, output,
#            synthesis directive (Default or AreaOptimized_high), route (0/1).
if {[llength $argv] != 6} {error "Expected rtl atomic engine output directive route"}
set root [file normalize [file join [file dirname [info script]] ../..]]
lassign $argv rtl atomic engine out directive route
foreach var {rtl atomic engine out} {set $var [file normalize [set $var]]}
file mkdir $out
cd $out
set_param general.maxThreads 4
create_project -force lut_opt [file join $out project] -part xc7z020clg400-1
foreach f {timing controller_registers batch_registers atomic_hls_adapter atomic_mailbox_arbiter shared_scheduler target_age pwm shared_control mailbox_control shared_engine_transport motor_engine_adapter cleanup_sequencer integrated_control safety qcencoder encoder_snapshot motor_io monitor_heartbeat axi_host controller_axi} {
 add_files [file join $rtl ${f}.sv]
}
foreach dir [list $atomic $engine] {
 add_files [glob [file join $dir *.v]]
 foreach pattern {*.dat *.mem} {foreach f [glob -nocomplain [file join $dir $pattern]] {add_files $f}}
}
set_property top dmotor_controller_axi [current_fileset]
update_compile_order -fileset sources_1
synth_design -top dmotor_controller_axi -part xc7z020clg400-1 -mode out_of_context -directive $directive
create_clock -name controller_clk -period 50 [get_ports clk]
report_utilization -file [file join $out utilization.rpt]
report_utilization -hierarchical -file [file join $out hierarchy.rpt]
report_drc -checks {LUTLP-1} -file [file join $out loops.rpt]
if {[llength [get_drc_violations -quiet -filter {NAME =~ LUTLP*}]]} {error "Combinational loop"}
write_checkpoint -force [file join $out synthesized.dcp]
if {$route} {
 source [file join $root constraints controller_halt.xdc]
 opt_design
 place_design
 route_design
 report_utilization -file [file join $out routed_utilization.rpt]
 report_timing_summary -file [file join $out timing.rpt]
 report_drc -file [file join $out drc.rpt]
 write_checkpoint -force [file join $out routed.dcp]
}
puts "LUT_OPT_MEASUREMENT_PASS"
