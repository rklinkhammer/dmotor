# Current controller requires a PS executor, exercised through VPI co-simulation.
set root [file normalize [file join [file dirname [info script]] ../..]]
set bench controller_axi
if {$argc > 0} {set bench [lindex $argv 0]}
puts [exec python3 [file join $root tools build.py] test --bench $bench]
