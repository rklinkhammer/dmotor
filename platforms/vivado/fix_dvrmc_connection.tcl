# Run once with the current dvrmc block design open. Does not save automatically.
# Let SmartConnect see the AXI4-Lite endpoint and perform clock conversion.
set sc [get_bd_cells axi_smc]
set cc [get_bd_cells axi_clock_converter_0]
if {![llength $cc]} {error "Expected axi_clock_converter_0; inspect the current topology"}
set old_nets [get_bd_intf_nets -of_objects [get_bd_intf_pins -of_objects $cc]]
delete_bd_objs $old_nets
delete_bd_objs $cc
set_property CONFIG.NUM_CLKS 2 $sc
connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK1] [get_bd_pins axi_smc/aclk1]
connect_bd_intf_net [get_bd_intf_pins axi_smc/M04_AXI] [get_bd_intf_pins controller_axi_0/S_AXI]
validate_bd_design
if {[get_property CONFIG.PROTOCOL [get_bd_intf_pins axi_smc/M04_AXI]] ne "AXI4LITE"} {
    error "SmartConnect output protocol is not AXI4LITE"
}
if {[get_property CONFIG.MAX_BURST_LENGTH [get_bd_intf_pins axi_smc/M04_AXI]] != 1} {
    error "SmartConnect output burst length is not 1"
}
