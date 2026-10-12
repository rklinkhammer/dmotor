# Source in an already-open Vivado project (for example dvrmc.xpr).
set dmotor_root [file normalize [file join [file dirname [info script]] ../..]]
if {![llength [get_projects -quiet]]} {error "Open the target Vivado project first"}
set dmotor_repos [get_property ip_repo_paths [current_project]]
foreach block {mailbox_control shared_engine} {
    set repo [file join $dmotor_root build current_ip $block ip]
    if {![file exists [file join $repo component.xml]]} {error "Run export_current.tcl first"}
    if {[lsearch -exact $dmotor_repos $repo] < 0} {lappend dmotor_repos $repo}
}
set_property ip_repo_paths $dmotor_repos [current_project]
update_ip_catalog
puts "Added current DMOTOR component repositories without replacing existing repositories."
