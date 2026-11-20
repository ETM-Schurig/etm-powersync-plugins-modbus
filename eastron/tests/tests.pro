QT += testlib
QT -= gui
CONFIG += console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_netenergyintegrator

# Pure logic of the SDM72 net metering: neither nymea nor Modbus. What happens
# on the bus and in the energy engine is a matter for an acceptance test on an
# installation (net metering), not for these tests.
#   qmake6 && make check

INCLUDEPATH += ..

SOURCES += \
    tst_netenergyintegrator.cpp \
    ../netenergyintegrator.cpp

HEADERS += \
    ../netenergyintegrator.h
