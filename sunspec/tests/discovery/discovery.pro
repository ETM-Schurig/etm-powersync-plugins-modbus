include(../../../sunspec.pri)

QT += testlib network serialbus
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_sunspecdiscovery

# The real plugin against a SunSpec Modbus TCP server set up in the process,
# on the loopback interface. What the ../plugin and ../multiunit suites do not
# see, for lack of a reachable device: the discoveries themselves, hence the
# creation of child things, the restart with things in the configuration,
# and the states published from real registers. The revert timer of a
# Fronius storage (revertTime) is seen end to end: value received at
# creation of the child, refusal of a setpoint when it is disabled.
#
# No external device: the server listens on 127.0.0.1. Run inside an
# isolated network namespace (for example bwrap --unshare-net) to be sure.

CONFIG += link_pkgconfig
PKGCONFIG += nymea nymea-core

PLUGIN_DIR = $$PWD/../..
INCLUDEPATH += $$PLUGIN_DIR

# Accessors reserved for tests: see integrationpluginsunspec.h. The suite
# reruns a discovery on a thing's connection, as the plugin does on
# every reconnection.
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
    tst_sunspecdiscovery.cpp \
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
