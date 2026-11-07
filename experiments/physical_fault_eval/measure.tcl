# Isolated safety cone, not full controller or board resource accounting.
set out [file normalize [lindex $argv 0]]
set_param general.maxThreads 4
foreach top {baseline candidate} {
 create_project -in_memory -part xc7z020clg400-1
 read_verilog -sv [list $out/baseline_safety.sv $out/candidate_safety.sv $out/${top}.sv]
 synth_design -top $top -part xc7z020clg400-1 -mode out_of_context
 create_clock -period 50 -name clk [get_ports clk]
 report_utilization -file $out/${top}_synth.rpt
 opt_design
 place_design
 report_utilization -file $out/${top}_placed.rpt
 report_timing_summary -file $out/${top}_timing.rpt
 close_project
}
puts "PHYSICAL_FAULT_MEASUREMENT_PASS"
