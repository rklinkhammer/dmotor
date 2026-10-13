set root [file normalize [file join [file dirname [info script]] ../..]]
set out [file join $root build integrated_sim]
create_project -force integrated_sim $out -part xc7z020clg400-1
foreach f {timing batch_registers atomic_hls_adapter atomic_mailbox_arbiter shared_scheduler target_age pwm shared_control mailbox_control shared_engine_transport motor_engine_adapter integrated_control} {add_files [file join $root rtl ${f}.sv]}
foreach dir [list [file join $root build vitis_atomic hls syn verilog] [file join $root build shared_mailbox_copy build vitis_dispatcher hls syn verilog]] {
    add_files [glob [file join $dir *.v]]
    foreach pat {*.dat *.mem} {foreach f [glob -nocomplain [file join $dir $pat]] {add_files $f}}
}
add_files -fileset sim_1 [file join $root tests integrated_control_tb.sv]
set_property top dmotor_integrated_control [current_fileset]
set_property top integrated_control_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
update_compile_order -fileset sim_1
launch_simulation -mode behavioral
run all
close_sim
set f [open [file join $out integrated_sim.sim sim_1 behav xsim simulate.log] r]
set result [read $f];close $f
if {![string match {*integrated_control PASS:*} $result]} {error "Integrated test did not PASS"}
close_project
