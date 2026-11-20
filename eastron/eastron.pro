include(../plugins.pri)

# Generate modbus connections
MODBUS_CONNECTIONS += sdm630-registers.json
MODBUS_CONNECTIONS += sdm72-registers.json
MODBUS_CONNECTIONS += sdm120-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationplugineastron.h \
    netenergyintegrator.h

SOURCES += \
    integrationplugineastron.cpp \
    netenergyintegrator.cpp
