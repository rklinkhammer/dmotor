# Historical resource and control-partition experiments

Main now builds the **1 ms PSF1 executor directly**. Use
[the product build guide](../docs/build.md), not preparation patches here.

The LUT, stripped PS prototype and full parallel-path directories retain their
measurements and evaluation code. Preparation scripts were written for the
pre-promotion source layout: reproduce those experiments from commit `f334736`
on `dmotor_mipi_opt`, in a separate checkout. Applying those patches to current
main is unsupported. In particular, the stripped prototype is not the product,
and the evaluated 5 ms profile is not selected.

Current execution is in `software/include/dmotor_sw/ps_engine.hpp` and
`software/linux/ps_device.cpp`; production tests and runners are in `tests/`
and `tools/`. Historical reports are not measurements of the rebuilt main board.
