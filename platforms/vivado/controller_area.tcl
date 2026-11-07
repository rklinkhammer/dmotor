# Apply only the measured synthesis directive to the controller's OOC run.
# Do not select the broader Flow_AreaOptimized_high strategy or change clocks.
# The caller has already generated BD targets; this creates runs but launches none.
proc dmotor_controller_area {bd_file} {
    create_ip_run $bd_file
    set runs [get_runs -quiet *_controller_axi_*_synth_1]
    if {[llength $runs] != 1} {
        error "Expected one controller OOC synthesis run; found: $runs"
    }
    set_property STEPS.SYNTH_DESIGN.ARGS.DIRECTIVE AreaOptimized_high $runs
    if {[get_property STEPS.SYNTH_DESIGN.ARGS.DIRECTIVE $runs] ne "AreaOptimized_high"} {
        error "Controller area directive was not applied"
    }
    puts "DMOTOR_CONTROLLER_AREA_PASS: $runs = AreaOptimized_high"
}
