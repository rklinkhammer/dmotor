# Recreate an editing project or a validated build project from saved source.
if {[llength $argv] ni {1 2}} {error "Expected output directory and optional edit|build mode"}
set mode [lindex $argv 1]
if {$mode eq ""} {set mode edit}
if {$mode ni {edit build}} {error "Unknown mode: $mode"}
set root [file normalize [file join [file dirname [info script]] ../../..]]
set output [file normalize [lindex $argv 0]]
if {[file exists [file join $output zybo-z7-20.xpr]]} {error "Editing project already exists; preserve BD/XDC edits and choose a new output directory"}
set ipdir [file join $root build current_ip controller_axi ip]
if {![file exists [file join $ipdir component.xml]]} {error "Build controller IP first: python3 tools/build.py ip"}
set check_args {}
if {$mode eq "build"} {lappend check_args --require-pins}
puts [exec python3 [file join $root tools check_zybo_interface.py] {*}$check_args]
create_project -force zybo-z7-20 $output -part xc7z020clg400-1
set_property target_language Verilog [current_project]
set board_repos [glob -nocomplain [file normalize ~/.Xilinx/Vivado/[version -short]/xhub/board_store/xilinx_board_store/XilinxBoardStore/Vivado/[version -short]/boards]]
if {[info exists ::env(DMOTOR_BOARD_REPO)]} {lappend board_repos $::env(DMOTOR_BOARD_REPO)}
if {[llength $board_repos]} {set_param board.repoPaths $board_repos}
set board digilentinc.com:zybo-z7-20:part0:1.2
if {![llength [get_board_parts -quiet $board]]} {error "Install Digilent Zybo Z7-20 board files 1.2 or set DMOTOR_BOARD_REPO"}
set_property board_part $board [current_project]
set_property ip_repo_paths [list $ipdir] [current_project]
update_ip_catalog
# Saved PS settings contain the selected 1 GiB DDR preset and original MIO setup.
# Do not reapply full board automation: it would overwrite user peripheral choices.
import_files -norecurse [file join $root platforms zybo-z7-20 vivado design_1.bd]
open_bd_design [get_files */design_1.bd]
if {[llength [get_bd_cells -quiet controller_axi_0]]} {
    upgrade_bd_cells [get_bd_cells controller_axi_0]
} else {
    set definitions [lsort [get_ipdefs -all dmotor.org:user:controller_axi:*]]
    if {![llength $definitions]} {error "Controller IP missing from catalog"}
    create_bd_cell -type ip -vlnv [lindex $definitions end] controller_axi_0
}
# Copy constraints into the editing project so GUI edits do not mutate source.
import_files -fileset constrs_1 -norecurse [file join $root platforms zybo-z7-20 constraints zybo-z7-20.xdc]
regenerate_bd_layout
if {$mode eq "build"} {
    foreach pin {clk reset qa qb physical_faults halt_async hb_async maintenance_async} {
        if {![llength [get_bd_nets -quiet -of_objects [get_bd_pins controller_axi_0/$pin]]]} {error "Unconnected controller input: $pin"}
    }
    foreach bus [get_bd_intf_pins controller_axi_0/*] {
        if {![llength [get_bd_intf_nets -quiet -of_objects $bus]]} {error "Unconnected controller interface: $bus"}
    }
    validate_bd_design
}
save_bd_design
if {$mode eq "build"} {
    generate_target all [get_files */design_1.bd]
    source [file join $root platforms vivado controller_area.tcl]
    dmotor_controller_area [get_files */design_1.bd]
    add_files [make_wrapper -files [get_files */design_1.bd] -top]
    set_property top design_1_wrapper [current_fileset]
    update_compile_order -fileset sources_1
}
report_ip_status -file [file join $output ip_status.txt]
puts "ZYBO_EDIT_PROJECT_READY: [file join $output zybo-z7-20.xpr]"
puts "ZYBO_PROJECT_MODE: $mode (synthesis and implementation are separate workflow stages)"
close_project
