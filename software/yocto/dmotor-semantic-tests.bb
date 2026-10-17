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
FILES:dmotor-runtime = "${bindir}/dmotord ${bindir}/dmotor-maint ${datadir}/dmotor/inhibited-example.conf"
RDEPENDS:dmotor-runtime += "dmotor-version"
