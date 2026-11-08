SUMMARY = "DMOTOR deterministic software semantic tests (no hardware access)"
LICENSE = "CLOSED"
SRC_URI = "file://dmotor-software.tar.gz"
S = "${WORKDIR}/dmotor-software"
OECMAKE_SOURCEPATH = "${S}/software"
inherit cmake qemu
DEPENDS += "qemu-native"
EXTRA_OECMAKE += "-DDMOTOR_SW_SANITIZE=OFF"
# Use BitBake's ARM toolchain, target sysroot and native QEMU. No custom PL model
# or board boot is implied by this user-mode emulation task.
do_semantics() {
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-semantic-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-protocol-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-linux-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-runtime-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-monitor-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-ps-worker-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-ps-cadence-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/dmotor-ps-fused-tests
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/esp32-tests/acquisition_test
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/esp32-tests/timing_cycle_test
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/esp32-tests/acquisition_link_test
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/esp32-tests/sensor_safety_test
    ${@qemu_wrapper_cmdline(d, d.getVar('RECIPE_SYSROOT'), [d.getVar('RECIPE_SYSROOT') + d.getVar('libdir'), d.getVar('RECIPE_SYSROOT') + d.getVar('base_libdir')])} ${B}/esp32-tests/monitor_policy_test
}
addtask semantics after do_compile before do_install

# Inspection is opt-in, separate from test executables and never auto-started.
PACKAGES =+ "dmotor-inspect"
FILES:dmotor-inspect = "${bindir}/dmotor-inspect"
PACKAGES =+ "dmotor-version"
FILES:dmotor-version = "${datadir}/dmotor/source-version.json"
RDEPENDS:${PN} += "dmotor-version"
RDEPENDS:dmotor-inspect += "dmotor-version"

PACKAGES =+ "dmotor-encoder"
FILES:dmotor-encoder = "${bindir}/dmotor-encoder"
RDEPENDS:dmotor-encoder += "dmotor-version"

# Opt-in commissioning service and separate maintenance client; no autostart.
PACKAGES =+ "dmotor-runtime"
FILES:dmotor-runtime = "${bindir}/dmotord ${bindir}/dmotor-maint ${bindir}/dmotor-uart-test ${bindir}/dmotor-acquisition-test ${datadir}/dmotor/inhibited-example.conf ${datadir}/dmotor/output-validation.conf ${datadir}/dmotor/powered-direct-duty.conf ${datadir}/dmotor/monitor"
RDEPENDS:dmotor-runtime += "dmotor-version"

# Client SDK: static library and standalone public header/CMake imported target.
# Runtime clients link this library; no daemon internals or MMIO headers are exported.
PACKAGES =+ "dmotor-client-staticdev dmotor-client-dev"
FILES:dmotor-client-staticdev = "${libdir}/libdmotor_client.a"
FILES:dmotor-client-dev = "${includedir}/dmotor_sw/client.hpp ${libdir}/cmake/dmotorClient"
RDEPENDS:dmotor-client-staticdev = "dmotor-client-dev (= ${EXTENDPKGV})"
RDEPENDS:dmotor-client-dev = ""
