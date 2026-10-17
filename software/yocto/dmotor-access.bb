SUMMARY = "Exclusive DMOTOR access and shared GPIO HALT"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://dmotor_access.c;beginline=1;endline=1;md5=50d2ba0afecd20f74c12a4bdbcfcfe61"
FILESEXTRAPATHS:prepend := "${THISDIR}/../dmotor-semantic-tests/files:"
SRC_URI = "file://dmotor-software.tar.gz"
S = "${WORKDIR}/dmotor-software/software/linux/driver"
inherit module
# Probe only with the opt-in dmotor,controller-0.18 overlay; never force UIO unbind.
