include(../../../sunspec.pri)

QT += testlib network serialbus
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_sunspecplugin

# Suite that really instantiates IntegrationPluginSunSpec, behind a
# real ThingManager (the one from libnymea-core) and a test-double HardwareManager.
# It covers what the pure logic suite cannot see: the dispatching of
# setupThing() / postSetupThing() / thingRemoved() on the thingClass.
#
# Additional dependencies compared with ../logic: libnymea1-dev and
# libnymea-core (pkg-config nymea and nymea-core).

CONFIG += link_pkgconfig
PKGCONFIG += nymea nymea-core

PLUGIN_DIR = $$PWD/../..
INCLUDEPATH += $$PLUGIN_DIR

# The plugin relies on the identifiers generated from its JSON. We
# generate them here exactly as plugin.pri does for the plugin itself:
# plugininfo.h (definitions, included by integrationpluginsunspec.cpp) and
# extern-plugininfo.h (declarations, included by the headers and by the test).
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
    tst_sunspecplugin.cpp \
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
