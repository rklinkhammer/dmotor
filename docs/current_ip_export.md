# Current IP export

Use **[the component build guide](build.md)**. `python3 tools/build.py ip` now
prepares the current shared profile, validates both HLS kernels, exports IP and
runs the full AXI simulation. EDF's complete build invokes it automatically.

The board uses `build/current_ip/controller_axi/ip` (`dmotor.org:user:controller_axi:0.18`).
Separate mailbox/engine packages remain available for specialist integration, but
are not additional board dependencies. Source constraints and topology now live
under `platforms/arty-z7-20`; do not manually add a sibling DVRMC IP repository.

Previous revision/measurement instructions are retained in the
[historical export log](history/ip-export-log.md). For interface/qualification
limits see [controller AXI](controller_axi.md).
