# Fresh synthesis of the complete controller; fail on combinational-loop DRC.
set root [file normalize [file join [file dirname [info script]] ../..]]
set out [file join $root build protection_loops project]
create_project -force protection_loops $out -part xc7z020clg400-1
foreach f {timing controller_registers batch_registers atomic_hls_adapter atomic_mailbox_arbiter shared_scheduler target_age pwm shared_control mailbox_control shared_engine_transport motor_engine_adapter integrated_control safety qcencoder encoder_snapshot motor_io axi_host controller_axi} {add_files [file join $root rtl ${f}.sv]}
foreach dir [list [file join $root build vitis_atomic hls syn verilog] [file join $root build shared_mailbox_copy build vitis_dispatcher hls syn verilog]] {
    add_files [glob [file join $dir *.v]]
    foreach pat {*.dat *.mem} {foreach f [glob -nocomplain [file join $dir $pat]] {add_files $f}}
}
set_property top dmotor_controller_axi [current_fileset]
update_compile_order -fileset sources_1
synth_design -top dmotor_controller_axi -part xc7z020clg400-1 -mode out_of_context
create_clock -name controller_clk -period 50 [get_ports clk]
write_checkpoint -force [file join $root build protection_loops controller.dcp]
report_utilization -hierarchical -file [file join $root build protection_loops utilization.txt]
report_drc -checks {LUTLP-1} -file [file join $root build protection_loops loops.txt]
set violations [get_drc_violations -quiet -filter {NAME =~ LUTLP*}]
if {[llength $violations]} {error "Combinational loops remain: $violations"}
puts "PROTECTION_LOOP_SYNTHESIS_PASS"
