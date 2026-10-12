# Separate PS7 reference project. Never modifies ~/workspace/dvrmc.
set root [file normalize [file join [file dirname [info script]] ../..]]
set out [file join $root build vivado_dev_bd]
set board_repo [file normalize ~/.Xilinx/Vivado/2026.1/xhub/board_store/xilinx_board_store]
if {[file isdirectory $board_repo]} {set_param board.repoPaths [list $board_repo]}
create_project -force dmotor_dev_bd $out -part xc7z020clg400-1
set board digilentinc.com:arty-z7-20:part0:1.1
if {![llength [get_board_parts -quiet $board]]} {error "Install the Arty Z7-20 board definition used by dvrmc"}
set_property board_part $board [current_project]
set_property ip_repo_paths [list [file join $root build vivado_dev ip]] [current_project]
update_ip_catalog
create_bd_design development
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:* ps7
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" apply_board_preset "1"} [get_bd_cells ps7]
# Dedicated development design: all PL AXI clocks use 20 MHz.
set_property -dict [list CONFIG.PCW_USE_M_AXI_GP0 {1} CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {20.000000}] [get_bd_cells ps7]
create_bd_cell -type ip -vlnv dmotor.org:user:dmotor_dev:1.0 dmotor
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Master "/ps7/M_AXI_GP0" Clk "Auto"} [get_bd_intf_pins dmotor/S_AXI]
assign_bd_address -offset 0x43C00000 -range 64K -target_address_space [get_bd_addr_spaces ps7/Data] [get_bd_addr_segs dmotor/registers/registers] -force
validate_bd_design
save_bd_design
generate_target all [get_files development.bd]
make_wrapper -files [get_files development.bd] -top -import
set f [open [file join $out addresses.txt] w]
foreach seg [get_bd_addr_segs -of_objects [get_bd_addr_spaces ps7/Data]] {
    puts $f "$seg [get_property OFFSET $seg] [get_property RANGE $seg]"
}
close $f
puts "Arty Z7-20 development BD validated. No motor pins or board bitstream generated."
