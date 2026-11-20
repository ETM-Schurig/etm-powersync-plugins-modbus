include(../../../sunspec.pri)

QT += testlib network serialbus
QT -= gui
TEMPLATE = app
TARGET = tst_sunspecdevice

# F3 integration suite: the plugin, set up behind a real ThingManager, against
# a real device.
#
# DELIBERATELY NOT "testcase": "make check" never runs it. It is
# built nonetheless, so that it does not silently rot.
#
# It requires a reachable device, passed via environment variable:
#
#   SUNSPEC_TEST_HOST=<adresse>   [SUNSPEC_TEST_PORT=502]
#   [SUNSPEC_TEST_METER_SLAVE_ID=200]  [SUNSPEC_TEST_SLAVE_ID=1]
#   ./tst_sunspecdevice
#
# Without SUNSPEC_TEST_HOST, every test goes to QSKIP. No default address
# is written here: the address of a real installation is customer data
# and has no business in a repository with a public mirror.
#
# WARNING before running it on a customer inverter: the manufacturer
# recommends staying above a 7 s polling interval, otherwise
# the inverter may go offline with respect to its portal, with
# recovery by an AC+DC power cut on site. This suite runs only one
# discovery cycle and does not poll in a loop, but it does open two connections.

CONFIG += console c++17
CONFIG -= app_bundle

CONFIG += link_pkgconfig
PKGCONFIG += nymea nymea-core

PLUGIN_DIR = $$PWD/../..
INCLUDEPATH += $$PLUGIN_DIR

DEFINES += SUNSPEC_PLUGIN_TEST_ACCESS

JSONFILE = $$PLUGIN_DIR/integrationpluginsunspec.json
OTHER_FILES += $$JSONFILE

plugininfo.input = JSONFILE
plugininfo.output = plugininfo.h
plugininfo.commands = nymea-plugininfocompiler ${QMAKE_FILE_NAME} --output ${QMAKE_FILE_OUT}
plugininfo.dependency_type = TYPE_C
plugininfo.CONFIG += no_link combine
QMAKE_EXTRA_COMPILERS += plugininfo

extern_plugininfo.input = JSONFILE
extern_plugininfo.output = extern-plugininfo.h
extern_plugininfo.commands = nymea-plugininfocompiler ${QMAKE_FILE_NAME} --extern ${QMAKE_FILE_OUT}
extern_plugininfo.dependency_type = TYPE_C
extern_plugininfo.CONFIG += no_link combine
QMAKE_EXTRA_COMPILERS += extern_plugininfo

DEFINES += PLUGIN_JSON_PATH=\\\"$$JSONFILE\\\"

SOURCES += \
    tst_sunspecdevice.cpp \
    $$PLUGIN_DIR/integrationpluginsunspec.cpp \
    $$PLUGIN_DIR/froniusrules.cpp \
    $$PLUGIN_DIR/solaredgebattery.cpp \
    $$PLUGIN_DIR/sunspecdiscovery.cpp \
    $$PLUGIN_DIR/sunspecduplicates.cpp \
    $$PLUGIN_DIR/sunspecstoragelogic.cpp \
    $$PLUGIN_DIR/sunspecthing.cpp

HEADERS += \
    $$PLUGIN_DIR/integrationpluginsunspec.h \
    $$PLUGIN_DIR/froniusrules.h \
    $$PLUGIN_DIR/solaredgebattery.h \
    $$PLUGIN_DIR/sunspecdiscovery.h \
    $$PLUGIN_DIR/sunspecduplicates.h \
    $$PLUGIN_DIR/sunspecstoragelogic.h \
    $$PLUGIN_DIR/sunspecthing.h
