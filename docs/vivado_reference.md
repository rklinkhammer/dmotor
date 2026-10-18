# Vivado integration reference

> Historical integration observation. The authoritative board sources now live in
> `platforms/arty-z7-20`; use the [current build guide](build.md).


Inspected 2026-10-11. The user-designated target reference is
`~/workspace/dvrmc/dvrmc.xpr`, with block design
`dvrmc.srcs/sources_1/bd/design_1/design_1.bd`. These files were read only.

| Item | Observed configuration |
|---|---|
| FPGA | `xc7z020clg400-1` |
| Board preset | `digilentinc.com:arty-z7-20:part0:1.1` (Arty Z7-20) |
| Top | `design_1_wrapper` |
| PS bus | `processing_system7_0/M_AXI_GP0` to `axi_smc/S00_AXI` |
| Connected PL clock | `FCLK_CLK0`, configured at 100 MHz |
| Reset | `FCLK_RESET0_N` through `rst_ps7_0_100M`; peripherals use `peripheral_aresetn` |
| Interrupts | GPIO2 and IIC through `ilconcat_0` to `IRQ_F2P` |
| Existing IP repository | `../dvrmc-portable/work/vitis_synthesis` |

Existing decoded segments, each 64 KiB: GPIO0 `0x41200000`, GPIO1
`0x41210000`, GPIO2 `0x41220000`, IIC `0x41600000`. DMOTOR has no assigned
base address or instance in the inspected block design. Its development register
offsets are peripheral-relative, not physical PS addresses.

The board preset identifies Arty Z7-20, not ZC702. Do not reuse ZC702 package,
pin, or board assumptions. The project exposes GPIO shield interfaces and IIC;
DMOTOR motor/encoder/HALT pin assignments and electrical constraints remain
integration work.

DMOTOR now targets 20 MHz. The inspected reference fabric remains 100 MHz;
changing this repository does not reconfigure that Vivado project. Supply a real
20 MHz clock to DMOTOR, constrain it accordingly, and synchronize reset in that
domain. If the AXI interconnect remains at 100 MHz, use a supported AXI clock
converter or correctly configured SmartConnect clock crossing. Do not connect
the 20 MHz design directly to the existing 100 MHz domain. Review encoder,
shared-HALT and other crossings during integration.

The controller retains a 1 kHz control tick (20,000 clocks), 20 kHz PWM
(1,000 clocks), and 800 us computation deadline (16,000 clocks). Clock/reset,
CDC and physical timing closure still require integration validation. Other PS
clock configuration fields do not imply connected clock nets.

Add the completed DMOTOR peripheral to SmartConnect with a non-overlapping address
segment, expand interrupts if needed, and define the bus drain/reset contract and
shared-HALT reset domain separately. Do not replace the existing IP repository
or overwrite the reference project as part of core development.
