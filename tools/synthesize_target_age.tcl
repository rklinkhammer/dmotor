# Standalone out-of-context synthesis. No board I/O or routed timing claim.
set root [file normalize [file join [file dirname [info script]] ..]]
set output [file join $root build target_age_synth]
file mkdir $output
set stream [open [file join $root rtl timing.sv] r]
set timing_text [read $stream]
close $stream
if {![regexp {localparam integer FABRIC_HZ = ([0-9]+);} $timing_text -> fabric_hz]} {
    error "Missing generated fabric clock"
}
read_verilog -sv [file join $root rtl timing.sv]
read_verilog -sv [file join $root rtl target_age.sv]
synth_design -top dmotor_target_age -part xc7z020clg400-1 -mode out_of_context
create_clock -name fabric -period [expr {1.0e9 / $fabric_hz}] [get_ports clk]
# Ideal synchronous boundary assumptions; integration must replace these delays.
set_input_delay 0 -clock fabric [get_ports -filter {DIRECTION == IN && NAME != clk}]
set_output_delay 0 -clock fabric [get_ports -filter {DIRECTION == OUT}]
report_utilization -file [file join $output utilization.rpt]
report_timing_summary -file [file join $output timing.rpt]
write_checkpoint -force [file join $output target_age.dcp]
