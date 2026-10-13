# Scoped by Vivado to this controller IP. Cut only the asynchronous HALT
# launch -> first-stage D path. The second stage and all safety logic stay timed.
set dmotor_halt_first [get_cells -hierarchical -filter {NAME =~ */halt_meta_reg}]
if {[llength $dmotor_halt_first] != 1} {
    error "DMOTOR: expected exactly one shared HALT first-stage register"
}
set_false_path -to [get_pins -of_objects $dmotor_halt_first -filter {REF_PIN_NAME == D}]
unset dmotor_halt_first
