# Package the current real blocks, separately; no diagnostic endpoint substitution.
set root [file normalize [file join [file dirname [info script]] ../..]]
set out [file join $root build current_ip]
file mkdir $out
proc export_block {root out name top generated sources} {
    foreach directory $generated {if {![file exists $directory]} {error "Missing generated HLS RTL: $directory"}}
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
    foreach directory $generated {
        add_files [glob [file join $directory *.v]]
        foreach pattern {*.dat *.mem} {
            foreach data [glob -nocomplain [file join $directory $pattern]] {add_files $data}
        }
    }
    set_property top $top [current_fileset]
    update_compile_order -fileset sources_1
    ipx::package_project -root_dir [file join $out $name ip] -vendor dmotor.org -library user -taxonomy /UserIP -import_files -set_current true
    set core [ipx::current_core]
    set_property name $name $core
    set_property version 0.18 $core
    # Scheduler feedback-loop repair: advertise a new revision to existing BDs.
    if {$name in {mailbox_control controller_axi}} {set_property core_revision 2 $core}
    if {$name eq "controller_axi"} {set_property core_revision 3 $core}
    set_property display_name "DMOTOR current $name (integration preview)" $core
    set_property description "Current tested component. Not a complete motor controller; see docs/current_ip_export.md." $core
    ipx::infer_bus_interfaces xilinx.com:signal:clock_rtl:1.0 $core
    ipx::infer_bus_interfaces xilinx.com:signal:reset_rtl:1.0 $core
    if {$name eq "controller_axi"} {
        foreach old [ipx::get_bus_interfaces -of_objects $core -filter {BUS_TYPE_NAME == aximm}] {
            ipx::remove_bus_interface [get_property NAME $old] $core
        }
        foreach old [ipx::get_memory_maps -of_objects $core] {
            ipx::remove_memory_map [get_property NAME $old] $core
        }
        set bus [ipx::add_bus_interface S_AXI $core]
        set_property bus_type_vlnv xilinx.com:interface:aximm:1.0 $bus
        set_property abstraction_type_vlnv xilinx.com:interface:aximm_rtl:1.0 $bus
        set_property interface_mode slave $bus
        foreach port {AWADDR AWVALID AWREADY WDATA WSTRB WVALID WREADY BRESP BVALID BREADY ARADDR ARVALID ARREADY RDATA RRESP RVALID RREADY} {
            set map [ipx::add_port_map $port $bus];set_property physical_name [string tolower $port] $map
        }
        foreach {key value} {PROTOCOL AXI4LITE DATA_WIDTH 32 ADDR_WIDTH 16} {
            set param [ipx::add_bus_parameter $key $bus];set_property value $value $param
        }
        set mmap [ipx::add_memory_map registers $core]
        set block [ipx::add_address_block registers $mmap]
        set_property range 65536 $block;set_property width 32 $block;set_property usage register $block
        set_property slave_memory_map_ref registers $bus
        ipx::associate_bus_interfaces -busif S_AXI -clock clk $core
        set clockif [ipx::get_bus_interfaces clk -of_objects $core]
        set assoc [ipx::get_bus_parameters ASSOCIATED_BUSIF -of_objects $clockif]
        set_property value S_AXI $assoc
        set resetif [ipx::get_bus_interfaces reset -of_objects $core]
        set polarity [ipx::add_bus_parameter POLARITY $resetif]
        set_property value ACTIVE_HIGH $polarity
        set hz [ipx::get_bus_parameters FREQ_HZ -of_objects $clockif]
        if {![llength $hz]} {set hz [ipx::add_bus_parameter FREQ_HZ $clockif]}
        set_property value 20000000 $hz
    }
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
export_block $root $out controller_axi dmotor_controller_axi \
    [list [file join $root build vitis_atomic hls syn verilog] [file join $root build shared_mailbox_copy build vitis_dispatcher hls syn verilog]] \
    {timing.sv controller_registers.sv batch_registers.sv atomic_hls_adapter.sv atomic_mailbox_arbiter.sv shared_scheduler.sv target_age.sv pwm.sv shared_control.sv mailbox_control.sv shared_engine_transport.sv motor_engine_adapter.sv integrated_control.sv safety.sv qcencoder.sv encoder_snapshot.sv motor_io.sv axi_host.sv controller_axi.sv}
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
