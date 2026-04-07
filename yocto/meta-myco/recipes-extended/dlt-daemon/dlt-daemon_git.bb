SUMMARY = "MyCo cockpit logging platform based on COVESA DLT daemon"
DESCRIPTION = "Protocol-compatible AUTOSAR DLT V1/V2 daemon with platform extensions for secure forwarding and observability."
HOMEPAGE = "https://github.com/however-yir/dlt-daemon"
LICENSE = "MPL-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=98c7df52f1f37e2bc0f7f72f17143049"

SRC_URI = "git://github.com/however-yir/dlt-daemon.git;branch=master;protocol=https"
SRCREV = "${AUTOREV}"
PV = "3.0.1+git${SRCPV}"
S = "${WORKDIR}/git"

inherit cmake pkgconfig systemd

EXTRA_OECMAKE += "-DWITH_SYSTEMD=ON -DWITH_DLT_CONSOLE=ON -DWITH_DLT_EXAMPLES=OFF"

SYSTEMD_SERVICE:${PN} = "dlt-platform.service"

FILES:${PN} += " \
  ${systemd_unitdir}/system/dlt-platform.service \
  ${bindir}/dlt-healthcheck.sh \
  ${sysconfdir}/dlt/adaptor/plugins.conf \
"
