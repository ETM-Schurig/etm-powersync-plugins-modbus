// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2026, ETM-PowerSync
*
* This file is part of nymea.
*
* nymea is free software: you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* nymea is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* nymea. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* F3 - one froniusConnection thing, two unit IDs.
 *
 * The GEN24 answers for the inverter on unit ID 1 and for the Smart Meter on
 * unit ID 200 by default, behind the same host:port. The plugin therefore opens
 * two SunSpecConnections for a single thing.
 *
 * What this suite covers: creation, count, unit IDs, and above all
 * destruction - a meter connection that outlives its thing keeps a
 * socket open and signals pointing to a destroyed Thing.
 *
 * What it does not cover, for lack of a Modbus peer: discovery, creation
 * of child things, the sweep of orphans and the aggregation of the
 * connected state. See ../integration.
 *
 * Binary separate from ../plugin: the F1 suite there checks global call
 * counters that additional configured things would falsify.
 */

#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>

#include <nymeasettings.h>
#include <hardwaremanager.h>
#include <plugintimer.h>
#include <network/networkdevicediscovery.h>
#include <network/networkdevicemonitor.h>
#include <integrations/thingmanagerimplementation.h>
#include <integrations/thingsetupinfo.h>

#include <sunspecconnection.h>

#include "integrationpluginsunspec.h"

/* --------------------------------------------------------------------------
 * Hardware test doubles. The monitor knows no address and is never
 * reachable: no Modbus connection is therefore attempted.
 * -------------------------------------------------------------------------- */

class FakeNetworkDeviceMonitor : public NetworkDeviceMonitor
{
public:
    explicit FakeNetworkDeviceMonitor(QObject *parent = nullptr) : NetworkDeviceMonitor(parent) {}

    MacAddress macAddress() const override { return MacAddress(); }
    QString hostName() const override { return QString(); }
    QHostAddress address() const override { return QHostAddress(); }
    NetworkDeviceInfo::MonitorMode monitorMode() const override { return NetworkDeviceInfo::MonitorModeIp; }
    NetworkDeviceInfo networkDeviceInfo() const override { return NetworkDeviceInfo(); }
    bool reachable() const override { return false; }
    QDateTime lastSeen() const override { return QDateTime(); }
    uint pingRetries() const override { return 0; }
    void setPingRetries(uint) override {}
};

class FakeNetworkDeviceDiscovery : public NetworkDeviceDiscovery
{
public:
    explicit FakeNetworkDeviceDiscovery(QObject *parent = nullptr) : NetworkDeviceDiscovery(parent) {}

    int registerMonitorCount = 0;
    int unregisterMonitorCount = 0;

    NetworkDeviceDiscoveryReply *discover() override { return nullptr; }
    bool running() const override { return false; }

    NetworkDeviceMonitor *registerMonitor(Thing *) override
    {
        registerMonitorCount++;
        return new FakeNetworkDeviceMonitor(this);
    }

    void unregisterMonitor(NetworkDeviceMonitor *monitor) override
    {
        unregisterMonitorCount++;
        delete monitor;
    }

    PingReply *ping(const QHostAddress &, uint) override { return nullptr; }
    PingReply *ping(const QString &, uint) override { return nullptr; }
    MacAddressDatabaseReply *lookupMacAddress(const QString &) override { return nullptr; }
    MacAddressDatabaseReply *lookupMacAddress(const MacAddress &) override { return nullptr; }
    bool sendArpRequest(const QHostAddress &) override { return false; }
    NetworkDeviceInfos cache() const override { return NetworkDeviceInfos(); }

    bool available() const override { return true; }
    bool enabled() const override { return true; }

protected:
    void setEnabled(bool) override {}
};

class FakePluginTimer : public PluginTimer
{
public:
    explicit FakePluginTimer(int seconds, QObject *parent = nullptr)
        : PluginTimer(parent), m_interval(seconds) {}

    int interval() const override { return m_interval; }
    int currentTick() const override { return 0; }
    bool running() const override { return m_running; }

    void reset() override {}
    void start() override { m_running = true; }
    void stop() override { m_running = false; }
    void pause() override {}
    void resume() override {}

private:
    int m_interval = 0;
    bool m_running = false;
};

class FakePluginTimerManager : public PluginTimerManager
{
public:
    explicit FakePluginTimerManager(QObject *parent = nullptr) : PluginTimerManager(parent) {}

    PluginTimer *registerTimer(int seconds) override { return new FakePluginTimer(seconds, this); }
    void unregisterTimer(PluginTimer *timer) override { delete timer; }

    bool available() const override { return true; }
    bool enabled() const override { return true; }

protected:
    void setEnabled(bool) override {}
};

class FakeHardwareManager : public HardwareManager
{
public:
    explicit FakeHardwareManager(QObject *parent = nullptr)
        : HardwareManager(parent)
        , m_pluginTimerManager(new FakePluginTimerManager(this))
        , m_networkDeviceDiscovery(new FakeNetworkDeviceDiscovery(this))
    {}

    Radio433 *radio433() override { return nullptr; }
    PluginTimerManager *pluginTimerManager() override { return m_pluginTimerManager; }
    NetworkAccessManager *networkManager() override { return nullptr; }
    UpnpDiscovery *upnpDiscovery() override { return nullptr; }
    PlatformZeroConfController *zeroConfController() override { return nullptr; }
    BluetoothLowEnergyManager *bluetoothLowEnergyManager() override { return nullptr; }
    MqttProvider *mqttProvider() override { return nullptr; }
    I2CManager *i2cManager() override { return nullptr; }
    ZigbeeHardwareResource *zigbeeResource() override { return nullptr; }
    ZWaveHardwareResource *zwaveResource() override { return nullptr; }
    ModbusRtuHardwareResource *modbusRtuResource() override { return nullptr; }
    NetworkDeviceDiscovery *networkDeviceDiscovery() override { return m_networkDeviceDiscovery; }

    FakeNetworkDeviceDiscovery *monitors() const { return m_networkDeviceDiscovery; }

private:
    FakePluginTimerManager *m_pluginTimerManager = nullptr;
    FakeNetworkDeviceDiscovery *m_networkDeviceDiscovery = nullptr;
};

class TestableSunSpecPlugin : public IntegrationPluginSunSpec
{
public:
    using IntegrationPlugin::setMetaData;
};

/* -------------------------------------------------------------------------- */

class SunSpecMultiUnitTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_F3_fronius_connection_creates_two_connections();
    void test_F3_meter_connection_uses_the_meter_slave_id();
    void test_F3_meter_connection_stays_idle_until_the_primary_discovered();
    void test_F3_meter_slave_id_zero_creates_only_one_connection();
    void test_F3_meter_slave_id_colliding_with_the_inverter_is_refused();
    void test_F3_generic_connection_creates_no_meter_connection();
    void test_F3_solaredge_connection_creates_no_meter_connection();
    void test_F3_removal_destroys_both_connections();

private:
    QTemporaryDir m_pluginSearchDir;
    FakeHardwareManager *m_hardwareManager = nullptr;
    ThingManagerImplementation *m_thingManager = nullptr;
    TestableSunSpecPlugin *m_plugin = nullptr;

    void storeConnection(const ThingId &thingId, const ThingClassId &thingClassId,
                         const QString &name, const PluginId &pluginId, int meterSlaveId);
    Thing *awaitThing(const ThingId &thingId);
};

/* Already configured things, written into the settings before the
 * ThingManager starts. This is the path taken on every nymead start for an
 * already paired connection, and the only one that completes the setup without
 * a reachable device. */
static const ThingId FroniusWithMeterId    = ThingId("1f0a5c7e-3b42-4d18-9e6a-70c2d4b85f31");
static const ThingId FroniusWithoutMeterId = ThingId("2c7d9e14-8a56-4f03-b21d-63e8a95c4072");
static const ThingId FroniusCollidingId    = ThingId("3b8e2f60-95c1-42a7-8d34-51fa7e60b9c8");
static const ThingId GenericConnectionId   = ThingId("4a1c6d83-27e9-4b50-a6f2-8c05d31e7ba4");
static const ThingId SolarEdgeConnectionId = ThingId("5e2b7f49-6d38-41ca-b95e-0a4c8d71e263");

static const int MeterSlaveId = 200;
static const int InverterSlaveId = 1;

void SunSpecMultiUnitTest::storeConnection(const ThingId &thingId, const ThingClassId &thingClassId,
                                           const QString &name, const PluginId &pluginId, int meterSlaveId)
{
    NymeaSettings settings(NymeaSettings::SettingsRoleThings);
    settings.beginGroup("ThingConfig");
    settings.beginGroup(thingId.toString());
    settings.setValue("thingName", name);
    settings.setValue("pluginid", pluginId.toString());
    settings.setValue("thingClassId", thingClassId.toString());
    /* The ThingManager is set up without a LogEngine - its header is not part
     * of the public nymea-core API - so the thing must not log any
     * state. */
    settings.setValue("loggedStateTypeIds", QStringList());
    settings.setValue("loggedEventTypeIds", QStringList());
    settings.setValue("loggedActionTypeIds", QStringList());
    settings.beginGroup("Params");

    if (thingClassId == froniusConnectionThingClassId) {
        settings.setValue(froniusConnectionThingAddressParamTypeId.toString(), "192.0.2.1");
        settings.setValue(froniusConnectionThingPortParamTypeId.toString(), 502);
        settings.setValue(froniusConnectionThingSlaveIdParamTypeId.toString(), InverterSlaveId);
        settings.setValue(froniusConnectionThingMeterSlaveIdParamTypeId.toString(), meterSlaveId);
    } else if (thingClassId == solarEdgeConnectionThingClassId) {
        settings.setValue(solarEdgeConnectionThingAddressParamTypeId.toString(), "192.0.2.1");
        settings.setValue(solarEdgeConnectionThingPortParamTypeId.toString(), 502);
        settings.setValue(solarEdgeConnectionThingSlaveIdParamTypeId.toString(), InverterSlaveId);
    } else {
        settings.setValue(sunspecConnectionThingAddressParamTypeId.toString(), "192.0.2.1");
        settings.setValue(sunspecConnectionThingPortParamTypeId.toString(), 502);
        settings.setValue(sunspecConnectionThingSlaveIdParamTypeId.toString(), InverterSlaveId);
    }

    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.sync();
}

Thing *SunSpecMultiUnitTest::awaitThing(const ThingId &thingId)
{
    Thing *thing = nullptr;
    [&]() {
        QTRY_VERIFY_WITH_TIMEOUT((thing = m_thingManager->findConfiguredThing(thingId)) != nullptr, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(thing->setupStatus(), Thing::ThingSetupStatusComplete, 2000);
    }();
    return thing;
}

void SunSpecMultiUnitTest::initTestCase()
{
    /* Organisation name specific to this binary: the settings therefore cross
     * neither the user's nor those of the ../plugin suite. */
    QCoreApplication::setOrganizationName("nymea-test-sunspec-multiunit");

    NymeaSettings thingSettings(NymeaSettings::SettingsRoleThings);
    thingSettings.clear();
    NymeaSettings pluginSettings(NymeaSettings::SettingsRolePlugins);
    pluginSettings.clear();

    QVERIFY(m_pluginSearchDir.isValid());
    qputenv("NYMEA_PLUGINS_PATH", m_pluginSearchDir.path().toUtf8());

    QFile jsonFile(PLUGIN_JSON_PATH);
    QVERIFY2(jsonFile.open(QIODevice::ReadOnly), qPrintable(QString("Cannot open %1").arg(PLUGIN_JSON_PATH)));
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));

    const PluginMetadata metadata(document.object());
    QVERIFY2(metadata.isValid(), "metadonnee du plugin invalide");

    storeConnection(FroniusWithMeterId, froniusConnectionThingClassId, "Fronius avec compteur",
                    metadata.pluginId(), MeterSlaveId);
    storeConnection(FroniusWithoutMeterId, froniusConnectionThingClassId, "Fronius sans compteur",
                    metadata.pluginId(), 0);
    storeConnection(FroniusCollidingId, froniusConnectionThingClassId, "Fronius unit ID en conflit",
                    metadata.pluginId(), InverterSlaveId);
    storeConnection(GenericConnectionId, sunspecConnectionThingClassId, "SunSpec generique",
                    metadata.pluginId(), 0);
    storeConnection(SolarEdgeConnectionId, solarEdgeConnectionThingClassId, "SolarEdge",
                    metadata.pluginId(), 0);

    m_hardwareManager = new FakeHardwareManager(this);
    m_thingManager = new ThingManagerImplementation(m_hardwareManager, nullptr, QLocale(), this);

    m_plugin = new TestableSunSpecPlugin();
    m_plugin->setMetaData(metadata);
    m_thingManager->registerStaticPlugin(m_plugin);

    QVERIFY2(!m_thingManager->findThingClass(froniusConnectionThingClassId).id().isNull(),
             "la thingClass froniusConnection n'est pas connue du ThingManager");
}

void SunSpecMultiUnitTest::cleanupTestCase()
{
    delete m_thingManager;
    m_thingManager = nullptr;
}

/* The core of F3: a single thing, two connections. */
void SunSpecMultiUnitTest::test_F3_fronius_connection_creates_two_connections()
{
    Thing *thing = awaitThing(FroniusWithMeterId);
    QVERIFY(thing);

    QVERIFY2(m_plugin->testPrimaryConnection(FroniusWithMeterId),
             "la connexion onduleur n'a pas ete creee");
    QVERIFY2(m_plugin->testMeterConnection(FroniusWithMeterId),
             "la connexion compteur n'a pas ete creee alors que meterSlaveId vaut 200");

    QVERIFY(m_plugin->testPrimaryConnection(FroniusWithMeterId)
            != m_plugin->testMeterConnection(FroniusWithMeterId));
}

/* Two connections to the same host:port differ only by the unit ID.
 * If it is not taken from the parameter, both poll the inverter. */
void SunSpecMultiUnitTest::test_F3_meter_connection_uses_the_meter_slave_id()
{
    SunSpecConnection *primary = m_plugin->testPrimaryConnection(FroniusWithMeterId);
    SunSpecConnection *meter = m_plugin->testMeterConnection(FroniusWithMeterId);
    QVERIFY(primary && meter);

    QCOMPARE(static_cast<int>(primary->slaveId()), InverterSlaveId);
    QCOMPARE(static_cast<int>(meter->slaveId()), MeterSlaveId);

    // Same device, hence same host and same port.
    QCOMPARE(meter->hostAddress(), primary->hostAddress());
    QCOMPARE(meter->port(), primary->port());
}

/* Q4: the meter connection is set up only once the primary discovery has
 * finished. Two simultaneous SunSpec discoveries would be the heaviest
 * load this plugin ever puts on an inverter, and the cost of
 * caution is only a few seconds of pairing.
 *
 * Here the monitor is never reachable, so the primary never discovers:
 * the meter connection must exist without being connected. */
void SunSpecMultiUnitTest::test_F3_meter_connection_stays_idle_until_the_primary_discovered()
{
    SunSpecConnection *meter = m_plugin->testMeterConnection(FroniusWithMeterId);
    QVERIFY(meter);
    QVERIFY2(!meter->connected(),
             "la connexion compteur ne doit pas etre montee avant la fin de la decouverte primaire");
}

/* 0 is the documented way of saying "no meter behind this inverter". */
void SunSpecMultiUnitTest::test_F3_meter_slave_id_zero_creates_only_one_connection()
{
    Thing *thing = awaitThing(FroniusWithoutMeterId);
    QVERIFY(thing);

    QVERIFY(m_plugin->testPrimaryConnection(FroniusWithoutMeterId));
    QVERIFY2(!m_plugin->testMeterConnection(FroniusWithoutMeterId),
             "meterSlaveId a 0 ne doit ouvrir aucune connexion compteur");
}

/* A second socket on the unit ID already served by the inverter would rediscover
 * the same models and duplicate every child thing. */
void SunSpecMultiUnitTest::test_F3_meter_slave_id_colliding_with_the_inverter_is_refused()
{
    Thing *thing = awaitThing(FroniusCollidingId);
    QVERIFY(thing);

    QVERIFY(m_plugin->testPrimaryConnection(FroniusCollidingId));
    QVERIFY2(!m_plugin->testMeterConnection(FroniusCollidingId),
             "un meterSlaveId egal au slaveId de l'onduleur doit etre refuse");
}

/* Non-regression: multiple unit IDs are a Fronius peculiarity. */
void SunSpecMultiUnitTest::test_F3_generic_connection_creates_no_meter_connection()
{
    Thing *thing = awaitThing(GenericConnectionId);
    QVERIFY(thing);

    QVERIFY(m_plugin->testPrimaryConnection(GenericConnectionId));
    QVERIFY2(!m_plugin->testMeterConnection(GenericConnectionId),
             "le vendor generique garde exactement une connexion par thing");
}

/* Non-regression of the solarEdge vendor, the one whose behaviour no suite
 * covered. This is why the meter connection lives in a
 * separate table rather than in a QHash of lists: that way not a single
 * line of solarEdge changed, including the reverse lookup
 * m_sunSpecConnections.key() on which battery discovery depends. */
void SunSpecMultiUnitTest::test_F3_solaredge_connection_creates_no_meter_connection()
{
    Thing *thing = awaitThing(SolarEdgeConnectionId);
    QVERIFY(thing);

    QVERIFY(m_plugin->testPrimaryConnection(SolarEdgeConnectionId));
    QVERIFY2(!m_plugin->testMeterConnection(SolarEdgeConnectionId),
             "le vendor solarEdge garde exactement une connexion par thing");
}

/* thingRemoved() is a chain of else ifs whose first branch dealt with
 * the primary connection only. Without explicit cleanup, the meter connection
 * outlives its thing: socket open and signals pointing to a destroyed Thing.
 *
 * QSignalSpy on destroyed() rather than inspecting the table: it is the
 * lifetime of the object that is at stake, not ownership by the table. */
void SunSpecMultiUnitTest::test_F3_removal_destroys_both_connections()
{
    SunSpecConnection *primary = m_plugin->testPrimaryConnection(FroniusWithMeterId);
    SunSpecConnection *meter = m_plugin->testMeterConnection(FroniusWithMeterId);
    QVERIFY(primary && meter);

    QSignalSpy primaryDestroyed(primary, &QObject::destroyed);
    QSignalSpy meterDestroyed(meter, &QObject::destroyed);

    QCOMPARE(m_thingManager->removeConfiguredThing(FroniusWithMeterId), Thing::ThingErrorNoError);

    // Both are destroyed by deleteLater(), hence on the next loop iteration.
    QTRY_COMPARE_WITH_TIMEOUT(primaryDestroyed.count(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(meterDestroyed.count(), 1, 2000);

    QVERIFY(!m_plugin->testPrimaryConnection(FroniusWithMeterId));
    QVERIFY2(!m_plugin->testMeterConnection(FroniusWithMeterId),
             "la connexion compteur est restee dans la table apres la suppression du thing");
}

QTEST_MAIN(SunSpecMultiUnitTest)
#include "tst_sunspecmultiunit.moc"
