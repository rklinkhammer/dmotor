open_project build/monitor_board_check/vivado/dvrmc.xpr
launch_runs synth_1 -jobs 4
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {error "Synthesis failed: [get_property STATUS [get_runs synth_1]]"}
launch_runs impl_1 -to_step route_design -jobs 4
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {error "Implementation failed: [get_property STATUS [get_runs impl_1]]"}
open_run impl_1
set reports build/monitor_board_check/reports
file mkdir $reports
report_timing_summary -delay_type min_max -report_unconstrained -file $reports/timing_summary.rpt
report_drc -file $reports/drc.rpt
report_cdc -details -file $reports/cdc.rpt
report_utilization -file $reports/utilization.rpt
report_io -file $reports/io.rpt
report_route_status -file $reports/route_status.rpt
report_exceptions -file $reports/exceptions.rpt
close_project
