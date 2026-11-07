# Route a measured complete-controller OOC checkpoint. Does not qualify board IO.
if {[llength $argv] != 2} {error "Expected synthesized-checkpoint output-directory"}
set root [file normalize [file join [file dirname [info script]] ../..]]
lassign $argv checkpoint out
set checkpoint [file normalize $checkpoint]
set out [file normalize $out]
file mkdir $out
cd $out
set_param general.maxThreads 4
open_checkpoint $checkpoint
source [file join $root constraints controller_halt.xdc]
opt_design
place_design
route_design
report_utilization -file [file join $out utilization.rpt]
report_timing_summary -file [file join $out timing.rpt]
report_drc -file [file join $out drc.rpt]
write_checkpoint -force [file join $out routed.dcp]
foreach delay {max min} {
 set paths [get_timing_paths -delay_type $delay -max_paths 1]
 if {![llength $paths] || [get_property SLACK $paths] < 0} {error "$delay timing failed"}
}
if {[llength [get_drc_violations -filter {SEVERITY == Error}]]} {error "DRC errors"}
puts "LUT_OPT_ROUTED_PASS"
