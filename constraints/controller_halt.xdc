# Scoped to the controller IP. Cut only asynchronous launch -> first-stage D.
# Keep stage-to-stage and downstream safety timing; validate topology in Tcl tests.
set_false_path -to [get_pins -of_objects [get_cells -hierarchical -filter {NAME =~ */halt_meta_reg}] -filter {REF_PIN_NAME == D}]
