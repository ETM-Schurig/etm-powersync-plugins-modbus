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

/* Suite that really instantiates the plugin.
 *
 * The pure logic suite (../logic) can only see what the plugin
 * *decides*: it calls free functions and reads the JSON. It cannot
 * see the dispatching, that is, whether a thingClass is recognised by
 * setupThing(), postSetupThing() and thingRemoved(). A thingClass forgotten in
 * one of these three dispatches falls into a final Q_ASSERT_X: in debug the
 * process dies, in release the assert is compiled out of the binary and
 * setupThing() returns without ever calling info->finish(). Pairing then
 * stays blocked indefinitely, without the slightest message.
 *
 * This is exactly what happened to froniusConnection, and no test
 * could see it.
 *
 * The plugin is set up behind the real ThingManager of libnymea-core - it is
 * the one that builds the Things and ThingSetupInfos, and the one that calls
 * postSetupThing() - and behind a test-double HardwareManager. No network access
 * takes place: the fake monitor is never reachable, so the plugin never
 * attempts a Modbus connection.
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

#include "integrationpluginsunspec.h"

/* --------------------------------------------------------------------------
 * Hardware test doubles
 *
 * The monitor knows no address and is never reachable: this is the state
 * of a connection whose device has not yet been seen on
 * the network. The plugin must answer it, not block on it.
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

    /* registerMonitor() is called only by the "connection" branch of
     * setupThing(). Counting the calls therefore tells whether the plugin took this
     * branch, and not merely whether it ended up answering something. */
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

    /* The polling timer is armed by postSetupThing(). The counter therefore
     * tells whether postSetupThing() actually handled the connection. */
    int registerTimerCount = 0;

    PluginTimer *registerTimer(int seconds) override
    {
        registerTimerCount++;
        return new FakePluginTimer(seconds, this);
    }

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

    FakePluginTimerManager *timers() const { return m_pluginTimerManager; }
    FakeNetworkDeviceDiscovery *monitors() const { return m_networkDeviceDiscovery; }

private:
    FakePluginTimerManager *m_pluginTimerManager = nullptr;
    FakeNetworkDeviceDiscovery *m_networkDeviceDiscovery = nullptr;
};

/* setMetaData() is protected: the ThingManager normally fills it in from the
 * QPluginLoader. Here the plugin is instantiated directly, so the test passes it
 * the same metadata, read from the same JSON. */
class TestableSunSpecPlugin : public IntegrationPluginSunSpec
{
public:
    using IntegrationPlugin::setMetaData;
};

/* -------------------------------------------------------------------------- */

class SunSpecPluginTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_F1_stored_fronius_connection_completes_its_setup();
    void test_F1_stored_fronius_connection_is_removed_cleanly();
    void test_F1_added_fronius_connection_is_answered();
    void test_F1_added_fronius_connection_answers_like_the_generic_vendor();

private:
    QTemporaryDir m_pluginSearchDir;
    FakeHardwareManager *m_hardwareManager = nullptr;
    ThingManagerImplementation *m_thingManager = nullptr;

    ThingSetupInfo *addConnection(const ThingClassId &thingClassId, const QString &name);
};

/* Already configured thing, written into the settings before the
 * ThingManager starts. This is the path taken on every nymead start for an
 * already paired connection, and the only one that completes the setup without
 * a reachable device. */
static const ThingId StoredFroniusThingId = ThingId("4e9b1c2d-6f31-4a58-91a7-2b8c0d5e7f14");

void SunSpecPluginTest::initTestCase()
{
    /* "nymea-test" moves the settings to /tmp, away from the user's real
     * configuration. */
    QCoreApplication::setOrganizationName("nymea-test");

    NymeaSettings thingSettings(NymeaSettings::SettingsRoleThings);
    thingSettings.clear();
    NymeaSettings pluginSettings(NymeaSettings::SettingsRolePlugins);
    pluginSettings.clear();

    /* Empty search directory: the only plugin involved is the one the test
     * registers itself, not those installed on the machine. */
    QVERIFY(m_pluginSearchDir.isValid());
    qputenv("NYMEA_PLUGINS_PATH", m_pluginSearchDir.path().toUtf8());

    QFile jsonFile(PLUGIN_JSON_PATH);
    QVERIFY2(jsonFile.open(QIODevice::ReadOnly), qPrintable(QString("Cannot open %1").arg(PLUGIN_JSON_PATH)));
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));

    const PluginMetadata metadata(document.object());
    QVERIFY2(metadata.isValid(), "metadonnee du plugin invalide");

    NymeaSettings settings(NymeaSettings::SettingsRoleThings);
    settings.beginGroup("ThingConfig");
    settings.beginGroup(StoredFroniusThingId.toString());
    settings.setValue("thingName", "Fronius");
    settings.setValue("pluginid", metadata.pluginId().toString());
    settings.setValue("thingClassId", froniusConnectionThingClassId.toString());
    /* The ThingManager is set up without a LogEngine - its header is not part
     * of the public nymea-core API - so the thing must not log any
     * state, otherwise the ThingManager would register a logger on a missing
     * engine. These three keys override the values suggested by the thingClass. */
    settings.setValue("loggedStateTypeIds", QStringList());
    settings.setValue("loggedEventTypeIds", QStringList());
    settings.setValue("loggedActionTypeIds", QStringList());
    settings.beginGroup("Params");
    settings.setValue(froniusConnectionThingAddressParamTypeId.toString(), "192.0.2.1");
    settings.setValue(froniusConnectionThingPortParamTypeId.toString(), 502);
    settings.setValue(froniusConnectionThingSlaveIdParamTypeId.toString(), 1);
    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.sync();

    m_hardwareManager = new FakeHardwareManager(this);
    m_thingManager = new ThingManagerImplementation(m_hardwareManager, nullptr, QLocale(), this);

    TestableSunSpecPlugin *plugin = new TestableSunSpecPlugin();
    plugin->setMetaData(metadata);
    m_thingManager->registerStaticPlugin(plugin);

    QVERIFY2(!m_thingManager->findThingClass(froniusConnectionThingClassId).id().isNull(),
             "la thingClass froniusConnection n'est pas connue du ThingManager");
}

void SunSpecPluginTest::cleanupTestCase()
{
    delete m_thingManager;
    m_thingManager = nullptr;
}

ThingSetupInfo *SunSpecPluginTest::addConnection(const ThingClassId &thingClassId, const QString &name)
{
    const ThingClass thingClass = m_thingManager->findThingClass(thingClassId);

    ParamList params;
    params.append(Param(thingClass.paramTypes().findByName("address").id(), QString("192.0.2.1")));

    return m_thingManager->addConfiguredThing(thingClassId, params, name);
}

/* The nymead startup path: the connection is already in the settings,
 * the setup is not an initial setup and must succeed without a reachable device.
 * This is the only case where we can check that success is actually announced.
 *
 * Before the fix, setupThing() does not recognise froniusConnection, never
 * finishes, and the thing stays in ThingSetupStatusInProgress forever. */
void SunSpecPluginTest::test_F1_stored_fronius_connection_completes_its_setup()
{
    Thing *thing = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((thing = m_thingManager->findConfiguredThing(StoredFroniusThingId)) != nullptr, 2000);
    QCOMPARE(thing->thingClassId(), froniusConnectionThingClassId);

    QTRY_COMPARE_WITH_TIMEOUT(thing->setupStatus(), Thing::ThingSetupStatusComplete, 2000);
    QCOMPARE(thing->setupError(), Thing::ThingErrorNoError);

    /* The connection branch of setupThing() was indeed taken... */
    QCOMPARE(m_hardwareManager->monitors()->registerMonitorCount, 1);
    /* ...and postSetupThing() armed the polling timer. */
    QCOMPARE(m_hardwareManager->timers()->registerTimerCount, 1);
}

/* thingRemoved() is indexed on m_sunSpecConnections, not on the thingClass: it
 * therefore has nothing specific to do for Fronius. Verified rather than assumed. */
void SunSpecPluginTest::test_F1_stored_fronius_connection_is_removed_cleanly()
{
    QVERIFY(m_thingManager->findConfiguredThing(StoredFroniusThingId));

    const int unregisteredBefore = m_hardwareManager->monitors()->unregisterMonitorCount;

    QCOMPARE(m_thingManager->removeConfiguredThing(StoredFroniusThingId), Thing::ThingErrorNoError);

    QVERIFY(!m_thingManager->findConfiguredThing(StoredFroniusThingId));
    QCOMPARE(m_hardwareManager->monitors()->unregisterMonitorCount, unregisteredBefore + 1);
}

/* The pairing path. Without a known address, an initial setup ends in
 * ThingErrorHardwareFailure: an explicit refusal, not a blockage. Before
 * the fix, no answer ever arrives. */
void SunSpecPluginTest::test_F1_added_fronius_connection_is_answered()
{
    const int monitorsBefore = m_hardwareManager->monitors()->registerMonitorCount;

    ThingSetupInfo *info = addConnection(froniusConnectionThingClassId, "Fronius appaire");
    QVERIFY(info);

    if (!info->isFinished()) {
        QSignalSpy finished(info, &ThingSetupInfo::finished);
        QVERIFY2(finished.wait(2000), "setupThing() n'a jamais appele info->finish()");
    }

    QCOMPARE(m_hardwareManager->monitors()->registerMonitorCount, monitorsBefore + 1);
    QCOMPARE(info->status(), Thing::ThingErrorHardwareFailure);
    QVERIFY(!info->displayMessage().isEmpty());
}

/* The generic vendor goes through exactly the same path. If the two answers
 * diverge, froniusConnection has been wired differently, not like the
 * vendors that already work. */
void SunSpecPluginTest::test_F1_added_fronius_connection_answers_like_the_generic_vendor()
{
    ThingSetupInfo *generic = addConnection(sunspecConnectionThingClassId, "SunSpec appaire");
    QVERIFY(generic);
    if (!generic->isFinished()) {
        QSignalSpy finished(generic, &ThingSetupInfo::finished);
        QVERIFY2(finished.wait(2000), "setupThing() n'a jamais appele info->finish() pour sunspecConnection");
    }

    ThingSetupInfo *fronius = addConnection(froniusConnectionThingClassId, "Fronius appaire 2");
    QVERIFY(fronius);
    if (!fronius->isFinished()) {
        QSignalSpy finished(fronius, &ThingSetupInfo::finished);
        QVERIFY2(finished.wait(2000), "setupThing() n'a jamais appele info->finish() pour froniusConnection");
    }

    QCOMPARE(fronius->status(), generic->status());
    QCOMPARE(fronius->displayMessage(), generic->displayMessage());
}

QTEST_MAIN(SunSpecPluginTest)
#include "tst_sunspecplugin.moc"
