# Complete experimental PSE1 endpoint, not a feature-equivalent product bitstream.
if {[llength $argv]!=1} {error "output directory required"}
set root [file normalize [file join [file dirname [info script]] ../..]]
set out [file normalize [lindex $argv 0]]
file mkdir $out
cd $out
set_param general.maxThreads 4
create_project -force ps_trial [file join $out project] -part xc7z020clg400-1
foreach f {timing axi_host qcencoder encoder_snapshot pwm monitor_heartbeat} {add_files [file join $root rtl ${f}.sv]}
add_files [file join $root experiments ps_executor ps_controller_axi.sv]
set_property top ps_controller_axi [current_fileset]
update_compile_order -fileset sources_1
synth_design -top ps_controller_axi -part xc7z020clg400-1 -mode out_of_context -directive AreaOptimized_high
create_clock -name controller_clk -period 50 [get_ports clk]
# Only asynchronous launch to FIRST-stage D, never the synchronizer second stage.
set cells [get_cells -hierarchical -filter {NAME =~ *halt_meta_reg || NAME =~ *hb_meta_reg || NAME =~ *physical_meta_reg* || NAME =~ */meta_reg*}]
set_false_path -to [get_pins -of_objects $cells -filter {REF_PIN_NAME == D}]
report_utilization -file [file join $out synthesized_utilization.rpt]
opt_design
place_design
route_design
report_utilization -file [file join $out utilization.rpt]
report_utilization -hierarchical -file [file join $out hierarchy.rpt]
report_timing_summary -file [file join $out timing.rpt]
report_drc -file [file join $out drc.rpt]
foreach delay {max min} {
 set paths [get_timing_paths -delay_type $delay -max_paths 1]
 if {![llength $paths] || [get_property SLACK $paths]<0} {error "Timing failed"}
}
if {[llength [get_drc_violations -quiet -filter {SEVERITY == Error}]]} {error "DRC errors"}
write_checkpoint -force [file join $out routed.dcp]
puts "PS_TRIAL_ROUTED_PASS"
