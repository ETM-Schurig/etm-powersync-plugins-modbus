QT += network serialport serialbus

# libnymea-modbus from the system package (libnymea-modbus-dev) or via NYMEA_MODBUS_PATH.
CONFIG += link_pkgconfig
PKGCONFIG += nymea-modbus

OTHER_FILES += $${MODBUS_CONNECTIONS}

# Locate modbus-tool.pri: system install first, then NYMEA_MODBUS_PATH override
# (for builds against a local libnymea-modbus source tree).
# When MODBUS_CONNECTIONS is empty, the tool's for-loop is a
# no-op; we still need the file for the C++ standard CONFIG lines below.
MODBUS_TOOL_PRI = $$[QT_INSTALL_PREFIX]/include/nymea-modbus/modbus-tool.pri
!isEmpty(NYMEA_MODBUS_PATH): MODBUS_TOOL_PRI = $$NYMEA_MODBUS_PATH/modbus-tool.pri

exists($$MODBUS_TOOL_PRI) {
    include($$MODBUS_TOOL_PRI)
} else {
    warning("modbus-tool.pri not found at $$MODBUS_TOOL_PRI — MODBUS_CONNECTIONS code-gen disabled")
    greaterThan(QT_MAJOR_VERSION, 5): CONFIG *= c++17
    lessThan(QT_MAJOR_VERSION, 6):    CONFIG *= c++11
}
