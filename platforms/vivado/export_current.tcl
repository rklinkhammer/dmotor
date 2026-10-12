# Package the current real blocks, separately; no diagnostic endpoint substitution.
set root [file normalize [file join [file dirname [info script]] ../..]]
set out [file join $root build current_ip]
file mkdir $out
proc export_block {root out name top generated sources} {
    if {![file exists $generated]} {error "Missing generated HLS RTL: $generated"}
    create_project -force $name [file join $out $name project] -part xc7z020clg400-1
    # IP packager cannot parse package-qualified parameter defaults. Resolve only
    # those references in staged copies from the generated timing package.
    set f [open [file join $root rtl timing.sv] r]; set timing [read $f]; close $f
    set staged [file join $out $name staged]
    file mkdir $staged
    foreach source $sources {
        set f [open [file join $root rtl $source] r]; set text [read $f]; close $f
        foreach {whole key value} [regexp -all -inline {localparam integer ([A-Z_]+) = ([0-9]+);} $timing] {
            set text [string map [list "dmotor_timing::$key" $value] $text]
        }
        set f [open [file join $staged $source] w]; puts -nonewline $f $text; close $f
        add_files [file join $staged $source]
    }
    add_files [glob [file join $generated *.v]]
    foreach pattern {*.dat *.mem} {
        foreach data [glob -nocomplain [file join $generated $pattern]] {add_files $data}
    }
    set_property top $top [current_fileset]
    update_compile_order -fileset sources_1
    ipx::package_project -root_dir [file join $out $name ip] -vendor dmotor.org -library user -taxonomy /UserIP -import_files -set_current true
    set core [ipx::current_core]
    set_property name $name $core
    set_property version 0.18 $core
    set_property display_name "DMOTOR current $name (integration preview)" $core
    set_property description "Current tested component. Not a complete motor controller; see docs/current_ip_export.md." $core
    ipx::infer_bus_interfaces xilinx.com:signal:clock_rtl:1.0 $core
    ipx::infer_bus_interfaces xilinx.com:signal:reset_rtl:1.0 $core
    ipx::create_xgui_files $core
    ipx::update_checksums $core
    ipx::check_integrity $core
    ipx::save_core $core
    ipx::archive_core [file join $out ${name}.zip] $core
    close_project
}
export_block $root $out mailbox_control dmotor_mailbox_control \
    [file join $root build vitis_atomic hls syn verilog] \
    {timing.sv batch_registers.sv atomic_hls_adapter.sv atomic_mailbox_arbiter.sv shared_scheduler.sv target_age.sv pwm.sv shared_control.sv mailbox_control.sv}
export_block $root $out shared_engine dmotor_shared_engine_transport \
    [file join $root build shared_mailbox_copy build vitis_dispatcher hls syn verilog] \
    {timing.sv shared_engine_transport.sv}
# Simulation uses actual mailbox HLS RTL; motor results are behavioral in this bench.
open_project [file join $out mailbox_control project mailbox_control.xpr]
add_files -fileset sim_1 [file join $root tests mailbox_control_tb.sv]
set_property top mailbox_control_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
update_compile_order -fileset sim_1
launch_simulation -simset sim_1 -mode behavioral
run all
close_sim
set log_path [file join $out mailbox_control project mailbox_control.sim sim_1 behav xsim simulate.log]
set f [open $log_path r]; set result [read $f]; close $f
if {![string match {*mailbox_control PASS:*} $result]} {error "Host simulation did not report PASS: $log_path"}
close_project
puts "Current IP exported and SystemVerilog host simulation completed. Check PASS in simulate.log."
