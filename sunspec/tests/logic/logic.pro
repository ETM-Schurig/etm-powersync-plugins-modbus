include(../../../sunspec.pri)

QT += testlib
QT -= gui
CONFIG += console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_sunspec

# Pure logic suite: what the plugin *decides*. It loads neither nymea nor
# the plugin itself, so it requires only libnymea-sunspec. The suite that
# really instantiates the plugin lives in ../plugin/ with its own
# dependencies. Bus traffic is left to on-site tests (see "Not verifiable by
# the unit tests of this repository" in specs/fronius-sunspec.md).

INCLUDEPATH += ../..

# Path of the plugin definition JSON, for the criteria concerning the
# thingClasses and their value ranges (AC-1, AC-7, AC-8, AC-10).
DEFINES += PLUGIN_JSON_PATH=\\\"$$PWD/../../integrationpluginsunspec.json\\\"

SOURCES += \
    tst_sunspec.cpp \
    ../../sunspecstoragelogic.cpp \
    ../../sunspecduplicates.cpp \
    ../../froniusrules.cpp

HEADERS += \
    ../../sunspecstoragelogic.h \
    ../../sunspecduplicates.h \
    ../../froniusrules.h
