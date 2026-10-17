# vivado -mode batch -source prepare_dvrmc_integration.tcl -tclargs source.xpr output-directory
# Explicitly upgrade controller_axi in a copy; never save the source project.
if {[llength $argv] != 2} {error "Expected source.xpr output-directory"}
lassign $argv source output
set root [file normalize [file join [file dirname [info script]] ../..]]
set output [file normalize $output]
if {[file exists $output]} {error "Choose a new output directory"}
open_project $source
if {[get_property PART [current_project]] ne "xc7z020clg400-1"} {error "Unexpected target part"}
save_project_as -exclude_run_results dvrmc $output
close_project
open_project [file join $output dvrmc.xpr]
set repos {}
foreach name {mailbox_control shared_engine controller_axi} {
    lappend repos [file join $root build current_ip $name ip]
}
set_property ip_repo_paths $repos [current_project]
update_ip_catalog
open_bd_design [get_files */design_1.bd]
upgrade_bd_cells [get_bd_cells controller_axi_0]
validate_bd_design
save_bd_design
generate_target all [get_files */design_1.bd]
report_ip_status -file [file join $output ip_status.txt]
close_project
