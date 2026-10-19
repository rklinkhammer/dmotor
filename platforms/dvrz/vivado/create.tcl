# Recreate the GUI project from tracked BD/XDC and freshly packaged controller IP.
if {[llength $argv] != 1} {error "Expected output directory"}
set root [file normalize [file join [file dirname [info script]] ../../..]]
set output [file normalize [lindex $argv 0]]
create_project -force dvrz $output -part xc7z020clg400-1
set_property target_language Verilog [current_project]
set board_repos [glob -nocomplain [file normalize ~/.Xilinx/Vivado/[version -short]/xhub/board_store/xilinx_board_store/XilinxBoardStore/Vivado/[version -short]/boards]]
if {[info exists ::env(DMOTOR_BOARD_REPO)]} {lappend board_repos $::env(DMOTOR_BOARD_REPO)}
if {[llength $board_repos]} {set_param board.repoPaths $board_repos}
set board myir.com:mys-7z020:part0:2.1
if {![llength [get_board_parts -quiet $board]]} {error "Install MYIR Z-Turn board files 2.1 or set DMOTOR_BOARD_REPO"}
set_property board_part $board [current_project]
# The BD carries the configured PS parameters; no board preset is reapplied.
set_property ip_repo_paths [list [file join $root build current_ip controller_axi ip]] [current_project]
update_ip_catalog
import_files -norecurse [file join $root platforms dvrz vivado dvrz.bd]
open_bd_design [get_files */dvrz.bd]
upgrade_bd_cells [get_bd_cells controller_axi_0]
validate_bd_design
save_bd_design
add_files -fileset constrs_1 [file join $root platforms dvrz constraints dvrz.xdc]
generate_target all [get_files */dvrz.bd]
set wrapper [make_wrapper -files [get_files */dvrz.bd] -top]
add_files $wrapper
set_property top dvrz_wrapper [current_fileset]
update_compile_order -fileset sources_1
report_ip_status -file [file join $output ip_status.txt]
close_project
