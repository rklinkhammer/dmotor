# Current IP export

Main exports revision **13** of `dmotor.org:user:controller_axi:0.18` at
`build/current_ip/controller_axi/ip`. The 1 ms PSF1 implementation replaces motor
HLS computation with the C++ daemon worker and retains atomic mailbox HLS, CLN1,
heartbeat supervision, coherent PWM and all existing board ports.

`python3 tools/build.py ip` runs atomic HLS simulation/synthesis/co-simulation,
native/controller/PS software tests and full AXI/cleanup co-simulation before
packaging the controller. `project` also recreates the board project; EDF's
complete build invokes this pipeline and then routes the full board and builds
matched ARM software. See [build instructions](build.md) and
[current qualification and artifact status](system_status.md).

Sources are direct `src/`, `include/`, `rtl/` and `software/` inputs. No generated
motor profile or separate motor HLS IP is a product dependency. Board topology
and constraints remain in `platforms/arty-z7-20`.

Previous revision/measurement instructions remain in the
[historical export log](history/ip-export-log.md). For interfaces and remaining
qualification see [controller AXI](controller_axi.md) and [PSF1](ps_executor.md).
