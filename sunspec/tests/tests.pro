TEMPLATE = subdirs

# logic  : pure plugin logic, without nymea. Requires only libnymea-sunspec.
# plugin : really instantiates IntegrationPluginSunSpec behind a
#          ThingManager and a test-double HardwareManager. Additionally needs
#          libnymea1-dev and libnymea-core (pkg-config nymea, nymea-core).

# multiunit : same setup as plugin, but for a froniusConnection thing
#          holding two SunSpecConnections (F3). Separate binary so as not to
#          skew the global call counters checked by plugin.

# discovery : the real plugin against an in-process SunSpec Modbus TCP
#          server, on 127.0.0.1. Discoveries, creation of children,
#          restart, states published from real registers, revertTime
#          received by a Fronius storage and refusal of a setpoint when this
#          timer is disabled.

# integration : the plugin against a real device. Built but NEVER run
#          by "make check": it requires a reachable inverter, passed via
#          SUNSPEC_TEST_HOST. See integration/integration.pro.

SUBDIRS += \
    logic \
    plugin \
    multiunit \
    discovery \
    integration
