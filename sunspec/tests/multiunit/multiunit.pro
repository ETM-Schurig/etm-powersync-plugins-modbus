include(../../../sunspec.pri)

QT += testlib network serialbus
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_sunspecmultiunit

# F3 suite: a froniusConnection thing holds two SunSpecConnections, one per
# unit ID, on the same host:port.
#
# Binary separate from ../plugin rather than tests added to that one: the
# already configured things are written into the settings before the
# ThingManager starts, so adding some would falsify the call counters that the
# F1 suite checks. Each binary has its own organisation name, hence its
# own settings file, and does not interfere with the other.

CONFIG += link_pkgconfig
PKGCONFIG += nymea nymea-core

PLUGIN_DIR = $$PWD/../..
INCLUDEPATH += $$PLUGIN_DIR

# Accessors reserved for tests: see integrationpluginsunspec.h. Without them, the
# number of connections of a thing is not observable, both living behind
# the same network monitor and the same host:port.
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
    tst_sunspecmultiunit.cpp \
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
