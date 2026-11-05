set root [file normalize [file join [file dirname [info script]] ../..]]
set engine_profile [file join $root build shared_mailbox_copy]
if {[info exists ::env(DMOTOR_ENGINE_PROFILE)]} {
    set engine_profile [file normalize $::env(DMOTOR_ENGINE_PROFILE)]
}
set bench controller_axi
if {$argc > 0} {set bench [lindex $argv 0]}
if {$bench ni {controller_axi controller_cleanup}} {error "Unknown controller bench: $bench"}
set out [file join $root build ${bench}_sim]
create_project -force ${bench}_sim $out -part xc7z020clg400-1
foreach f {timing controller_registers batch_registers atomic_hls_adapter atomic_mailbox_arbiter shared_scheduler target_age pwm shared_control mailbox_control shared_engine_transport motor_engine_adapter cleanup_sequencer integrated_control safety qcencoder encoder_snapshot motor_io monitor_heartbeat axi_host controller_axi} {add_files [file join $root rtl ${f}.sv]}
foreach dir [list [file join $root build vitis_atomic hls syn verilog] [file join $engine_profile build vitis_dispatcher hls syn verilog]] {
    add_files [glob [file join $dir *.v]]
    foreach pat {*.dat *.mem} {foreach f [glob -nocomplain [file join $dir $pat]] {add_files $f}}
}
add_files -fileset sim_1 [file join $root tests ${bench}_tb.sv]
set_property top dmotor_controller_axi [current_fileset]
set_property top ${bench}_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
update_compile_order -fileset sim_1
launch_simulation -mode behavioral
run all
close_sim
set f [open [file join $out ${bench}_sim.sim sim_1 behav xsim simulate.log] r]
set result [read $f];close $f
if {![string match "*${bench} PASS:*" $result]} {error "Integrated test did not PASS"}
close_project
