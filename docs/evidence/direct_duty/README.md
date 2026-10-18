# Direct-duty commissioning validation

Software 0.8.0 / DMI2 / controller IP revision 7.

- Seven native UBSan suites pass, including direct-duty process tests, configurable
  floor/ceiling rejection, zero command, signed duty, rearm and timeout behavior.
- Full Vivado AXI simulation with the existing generated atomic/velocity HLS kernels
  passes legacy velocity tests plus encoder-free duty: 0/25/50/100%, signed ceiling,
  reversal, mode/limit write guards, replay rejection, immediate STOP, command age,
  authorization watchdog, external fault and external HALT.
- EDF ZC702 ARM build: 1214 tasks pass, including user-mode QEMU do_semantics and
  kernel-module compilation/packaging. This is not a Linux/PL board test.
- Full controller 20 MHz out-of-context routed check passed: setup WNS +13.648 ns,
  hold WHS +0.039 ns; zero DRC errors. DSP pipelining and OOC port-location warnings
  remain. External pin timing is not established by this OOC check.

No board bitstream or SD image was exported/deployed in this step. Powered motor
operation is not qualified. Use the central build workflow to create a matched
DMI2 bitstream, kernel module and software image before board commissioning.
