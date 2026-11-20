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

/* F3 against a real device.
 *
 * The two most certain defects of multiple unit IDs are invisible to the
 * offline suites, because they only appear once discovery has
 * actually completed:
 *
 *   - the sweep of orphans, which on only one of the two connections would
 *     make the children of the other one disappear on every discovery;
 *   - the aggregation of the connected state across the two connections.
 *
 * This suite is not run by "make check": it requires a device. See
 * integration.pro for the environment variables. Without SUNSPEC_TEST_HOST,
 * everything goes to QSKIP.
 *
 * No default address is written here. The address of a real installation
 * is customer data, which has no place in a public repository.
 */

#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>

#include <nymeasettings.h>
#include <hardwaremanager.h>
#include <plugintimer.h>
#include <logging/logengine.h>
#include <logging/logger.h>
#include <network/networkdevicediscovery.h>
#include <network/networkdevicemonitor.h>
#include <integrations/thingmanagerimplementation.h>
#include <integrations/thingsetupinfo.h>

#include <sunspecconnection.h>

#include "integrationpluginsunspec.h"

/* --------------------------------------------------------------------------
 * Test doubles
 * -------------------------------------------------------------------------- */

/* Unlike the offline suites, the monitor here carries a real address
 * and declares itself reachable: this is what allows the plugin to connect. */
class DeviceMonitor : public NetworkDeviceMonitor
{
public:
    DeviceMonitor(const QHostAddress &address, QObject *parent = nullptr)
        : NetworkDeviceMonitor(parent), m_address(address)
    {
        m_info = NetworkDeviceInfo(address);
        m_info.setMonitorMode(NetworkDeviceInfo::MonitorModeIp);
    }

    MacAddress macAddress() const override { return MacAddress(); }
    QString hostName() const override { return QString(); }
    QHostAddress address() const override { return m_address; }
    NetworkDeviceInfo::MonitorMode monitorMode() const override { return NetworkDeviceInfo::MonitorModeIp; }
    NetworkDeviceInfo networkDeviceInfo() const override { return m_info; }
    bool reachable() const override { return true; }
    QDateTime lastSeen() const override { return QDateTime::currentDateTime(); }
    uint pingRetries() const override { return 0; }
    void setPingRetries(uint) override {}

private:
    QHostAddress m_address;
    NetworkDeviceInfo m_info;
};

class DeviceDiscovery : public NetworkDeviceDiscovery
{
public:
    DeviceDiscovery(const QHostAddress &address, QObject *parent = nullptr)
        : NetworkDeviceDiscovery(parent), m_address(address) {}

    NetworkDeviceDiscoveryReply *discover() override { return nullptr; }
    bool running() const override { return false; }

    NetworkDeviceMonitor *registerMonitor(Thing *) override { return new DeviceMonitor(m_address, this); }
    void unregisterMonitor(NetworkDeviceMonitor *monitor) override { delete monitor; }

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

private:
    QHostAddress m_address;
};

class NullPluginTimer : public PluginTimer
{
public:
    explicit NullPluginTimer(int seconds, QObject *parent = nullptr)
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

/* The timer is never fired by this suite. Polling a customer inverter in a
 * loop is precisely what the polling-rate floor seeks to
 * avoid; here we only want the discovery. */
class NullPluginTimerManager : public PluginTimerManager
{
public:
    explicit NullPluginTimerManager(QObject *parent = nullptr) : PluginTimerManager(parent) {}

    PluginTimer *registerTimer(int seconds) override { return new NullPluginTimer(seconds, this); }
    void unregisterTimer(PluginTimer *timer) override { delete timer; }

    bool available() const override { return true; }
    bool enabled() const override { return true; }

protected:
    void setEnabled(bool) override {}
};

/* The child things are created by the plugin, not written in advance into the
 * settings: we therefore cannot strip their logged states the way the
 * offline suites do. A LogEngine is needed, even an empty one, otherwise the
 * ThingManager dereferences a missing one as soon as the first child is created. */
class NullLogEngine : public LogEngine
{
public:
    explicit NullLogEngine(QObject *parent = nullptr) : LogEngine(parent) {}

    Logger *registerLogSource(const QString &name, const QStringList &tags,
                              Types::LoggingType loggingType, const QString &) override
    {
        Logger *logger = createLogger(name, tags, loggingType);
        m_loggers.insert(name, logger);
        return logger;
    }

    void unregisterLogSource(const QString &name) override { m_loggers.remove(name); }

    LogFetchJob *fetchLogEntries(const QStringList &, const QStringList &, const QDateTime &,
                                 const QDateTime &, const QVariantMap &, Types::SampleRate,
                                 Qt::SortOrder, int, int) override
    {
        LogFetchJob *job = new LogFetchJob(this);
        finishFetchJob(job, LogEntries());
        return job;
    }

    bool jobsRunning() const override { return false; }
    void clear(const QString &) override {}
    void enable() override {}
    void disable() override {}

private:
    void logEvent(Logger *, const QStringList &, const QVariantMap &) override {}

    QHash<QString, Logger *> m_loggers;
};

class DeviceHardwareManager : public HardwareManager
{
public:
    DeviceHardwareManager(const QHostAddress &address, QObject *parent = nullptr)
        : HardwareManager(parent)
        , m_pluginTimerManager(new NullPluginTimerManager(this))
        , m_networkDeviceDiscovery(new DeviceDiscovery(address, this))
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

private:
    NullPluginTimerManager *m_pluginTimerManager = nullptr;
    DeviceDiscovery *m_networkDeviceDiscovery = nullptr;
};

class TestableSunSpecPlugin : public IntegrationPluginSunSpec
{
public:
    using IntegrationPlugin::setMetaData;
};

/* -------------------------------------------------------------------------- */

class SunSpecDeviceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_F3_both_connections_reach_the_device();
    void test_F3_meter_connects_only_after_the_primary_discovery();
    void test_F3_inverter_and_meter_children_share_one_parent();
    void test_F3_no_child_disappears_across_repeated_discoveries();
    void test_F3_meter_slave_id_zero_opens_no_second_connection();
    void test_F3_losing_the_meter_leaves_the_inverter_alone();

private:
    QTemporaryDir m_pluginSearchDir;
    QHostAddress m_address;
    int m_port = 502;
    int m_slaveId = 1;
    int m_meterSlaveId = 200;

    DeviceHardwareManager *m_hardwareManager = nullptr;
    ThingManagerImplementation *m_thingManager = nullptr;
    NullLogEngine *m_logEngine = nullptr;
    TestableSunSpecPlugin *m_plugin = nullptr;

    bool m_configured = false;

    /* These two flags are written by lambdas connected in
     * initTestCase() and read back by a later test: they are therefore members,
     * never locals captured by reference. A local of initTestCase()
     * captured by reference would be released when the function returns, and the
     * lambdas would then write into a stack slot reused by the following
     * frames - silent corruption of the test itself. */
    bool m_primaryDiscovered = false;
    /* True as soon as the meter connection signals connected, to check that
     * this never happens before the end of the primary discovery. */
    bool m_meterConnectedBeforePrimaryDiscovery = false;

    bool skipUnlessConfigured();
    QList<Thing *> childrenOfKind(const ThingClassId &thingClassId) const;
    void storeConnection(const ThingId &thingId, const QString &name,
                         const PluginId &pluginId, int meterSlaveId);
    SunSpecModel *modelOn(SunSpecConnection *connection, quint16 modelId) const;
};

static const ThingId DeviceThingId = ThingId("5d3f8a26-41bc-4e79-a05f-9b7c2e6d1483");
static const ThingId DeviceNoMeterThingId = ThingId("6a4e1b07-52d8-4c93-8e7b-1f39c04a5d62");

void SunSpecDeviceTest::storeConnection(const ThingId &thingId, const QString &name,
                                        const PluginId &pluginId, int meterSlaveId)
{
    NymeaSettings settings(NymeaSettings::SettingsRoleThings);
    settings.beginGroup("ThingConfig");
    settings.beginGroup(thingId.toString());
    settings.setValue("thingName", name);
    settings.setValue("pluginid", pluginId.toString());
    settings.setValue("thingClassId", froniusConnectionThingClassId.toString());
    settings.beginGroup("Params");
    settings.setValue(froniusConnectionThingAddressParamTypeId.toString(), m_address.toString());
    settings.setValue(froniusConnectionThingPortParamTypeId.toString(), m_port);
    settings.setValue(froniusConnectionThingSlaveIdParamTypeId.toString(), m_slaveId);
    settings.setValue(froniusConnectionThingMeterSlaveIdParamTypeId.toString(), meterSlaveId);
    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.sync();
}

bool SunSpecDeviceTest::skipUnlessConfigured()
{
    if (m_configured)
        return false;

    /* QSKIP cannot be called from a helper function, so the
     * message is left to the caller. */
    return true;
}

QList<Thing *> SunSpecDeviceTest::childrenOfKind(const ThingClassId &thingClassId) const
{
    QList<Thing *> found;
    foreach (Thing *thing, m_thingManager->configuredThings()) {
        if (thing->thingClassId() == thingClassId && thing->parentId() == DeviceThingId)
            found.append(thing);
    }
    return found;
}

void SunSpecDeviceTest::initTestCase()
{
    const QByteArray host = qgetenv("SUNSPEC_TEST_HOST");
    if (host.isEmpty()) {
        qInfo() << "SUNSPEC_TEST_HOST is not set: every test will be skipped.";
        return;
    }

    m_address = QHostAddress(QString::fromUtf8(host));
    QVERIFY2(!m_address.isNull(), "SUNSPEC_TEST_HOST is not a valid address");

    if (!qgetenv("SUNSPEC_TEST_PORT").isEmpty())
        m_port = qgetenv("SUNSPEC_TEST_PORT").toInt();
    if (!qgetenv("SUNSPEC_TEST_SLAVE_ID").isEmpty())
        m_slaveId = qgetenv("SUNSPEC_TEST_SLAVE_ID").toInt();
    if (!qgetenv("SUNSPEC_TEST_METER_SLAVE_ID").isEmpty())
        m_meterSlaveId = qgetenv("SUNSPEC_TEST_METER_SLAVE_ID").toInt();

    QCoreApplication::setOrganizationName("nymea-test-sunspec-device");

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

    storeConnection(DeviceThingId, "Fronius", metadata.pluginId(), m_meterSlaveId);
    /* The degenerate case of meterSlaveId (AC-7): 0 = no meter behind this
     * inverter. Same device, same inverter unit ID, but no second
     * connection must be opened. */
    storeConnection(DeviceNoMeterThingId, "Fronius sans compteur", metadata.pluginId(), 0);

    m_hardwareManager = new DeviceHardwareManager(m_address, this);
    m_logEngine = new NullLogEngine(this);
    m_thingManager = new ThingManagerImplementation(m_hardwareManager, m_logEngine, QLocale(), this);

    m_plugin = new TestableSunSpecPlugin();
    m_plugin->setMetaData(metadata);
    m_thingManager->registerStaticPlugin(m_plugin);

    Thing *thing = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((thing = m_thingManager->findConfiguredThing(DeviceThingId)) != nullptr, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(thing->setupStatus(), Thing::ThingSetupStatusComplete, 30000);

    SunSpecConnection *primary = m_plugin->testPrimaryConnection(DeviceThingId);
    SunSpecConnection *meter = m_plugin->testMeterConnection(DeviceThingId);
    QVERIFY(primary);
    QVERIFY2(meter, "aucune connexion compteur creee : verifier SUNSPEC_TEST_METER_SLAVE_ID");

    /* Armed before any wait, so that the order of the two events is
     * observed and not reconstructed afterwards. */
    connect(primary, &SunSpecConnection::discoveryFinished, this, [this](bool success) {
        if (success)
            m_primaryDiscovered = true;
    });
    connect(meter, &SunSpecConnection::connectedChanged, this, [this](bool connected) {
        if (connected && !m_primaryDiscovered)
            m_meterConnectedBeforePrimaryDiscovery = true;
    });

    m_configured = true;
}

void SunSpecDeviceTest::cleanupTestCase()
{
    delete m_thingManager;
    m_thingManager = nullptr;
}

void SunSpecDeviceTest::test_F3_both_connections_reach_the_device()
{
    if (skipUnlessConfigured())
        QSKIP("SUNSPEC_TEST_HOST is not set");

    SunSpecConnection *primary = m_plugin->testPrimaryConnection(DeviceThingId);
    SunSpecConnection *meter = m_plugin->testMeterConnection(DeviceThingId);
    QVERIFY(primary && meter);

    QTRY_VERIFY_WITH_TIMEOUT(primary->connected(), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(meter->connected(), 60000);

    QCOMPARE(static_cast<int>(primary->slaveId()), m_slaveId);
    QCOMPARE(static_cast<int>(meter->slaveId()), m_meterSlaveId);

    // Both found a SunSpec base register, each on its own unit ID.
    QTRY_VERIFY_WITH_TIMEOUT(!primary->models().isEmpty(), 60000);
    QTRY_VERIFY_WITH_TIMEOUT(!meter->models().isEmpty(), 60000);
}

/* Q4. Two simultaneous SunSpec discoveries would be the heaviest load
 * this plugin ever puts on an inverter. */
void SunSpecDeviceTest::test_F3_meter_connects_only_after_the_primary_discovery()
{
    if (skipUnlessConfigured())
        QSKIP("SUNSPEC_TEST_HOST is not set");

    QVERIFY2(!m_meterConnectedBeforePrimaryDiscovery,
             "la connexion compteur est montee avant la fin de la decouverte primaire");
}

/* A single device to pair, all children under the same parent. */
void SunSpecDeviceTest::test_F3_inverter_and_meter_children_share_one_parent()
{
    if (skipUnlessConfigured())
        QSKIP("SUNSPEC_TEST_HOST is not set");

    QTRY_VERIFY_WITH_TIMEOUT(!childrenOfKind(sunspecThreePhaseInverterThingClassId).isEmpty()
                             || !childrenOfKind(sunspecSplitPhaseInverterThingClassId).isEmpty()
                             || !childrenOfKind(sunspecSinglePhaseInverterThingClassId).isEmpty(), 60000);

    QTRY_VERIFY_WITH_TIMEOUT(!childrenOfKind(sunspecThreePhaseMeterThingClassId).isEmpty()
                             || !childrenOfKind(sunspecSplitPhaseMeterThingClassId).isEmpty()
                             || !childrenOfKind(sunspecSinglePhaseMeterThingClassId).isEmpty(), 60000);
}

/* The most certain defect of multiple unit IDs: each connection sees only
 * half of the picture, so a sweep of orphans done on that half would make
 * the children of the other one disappear. A rediscovery is enough to trigger it. */
void SunSpecDeviceTest::test_F3_no_child_disappears_across_repeated_discoveries()
{
    if (skipUnlessConfigured())
        QSKIP("SUNSPEC_TEST_HOST is not set");

    const int childrenBefore = m_thingManager->configuredThings().filterByParentId(DeviceThingId).count();
    QVERIFY2(childrenBefore > 0, "aucun enfant a surveiller");

    QSignalSpy disappeared(m_plugin, &IntegrationPlugin::autoThingDisappeared);

    SunSpecConnection *primary = m_plugin->testPrimaryConnection(DeviceThingId);
    SunSpecConnection *meter = m_plugin->testMeterConnection(DeviceThingId);

    for (int round = 0; round < 2; round++) {
        QSignalSpy primaryDone(primary, &SunSpecConnection::discoveryFinished);
        primary->startDiscovery();
        QVERIFY2(primaryDone.wait(60000), "la decouverte primaire n'a pas rendu la main");

        QSignalSpy meterDone(meter, &SunSpecConnection::discoveryFinished);
        meter->startDiscovery();
        QVERIFY2(meterDone.wait(60000), "la decouverte compteur n'a pas rendu la main");
    }

    QCOMPARE(disappeared.count(), 0);
    QCOMPARE(m_thingManager->configuredThings().filterByParentId(DeviceThingId).count(), childrenBefore);
}

SunSpecModel *SunSpecDeviceTest::modelOn(SunSpecConnection *connection, quint16 modelId) const
{
    foreach (SunSpecModel *model, connection->models()) {
        if (model->modelId() == modelId)
            return model;
    }
    return nullptr;
}

/* The degenerate case: meterSlaveId at 0 means "no meter behind this
 * inverter". Same device, same inverter unit ID, but a single connection.
 *
 * We assert nothing about the children of this second thing: the first one already
 * created an inverter child carrying the simulator's serial number, and the
 * serial-number deduplication of autocreateSunSpecModelThing() is
 * global, not per parent. It is the connection that is measured here. */
void SunSpecDeviceTest::test_F3_meter_slave_id_zero_opens_no_second_connection()
{
    if (skipUnlessConfigured())
        QSKIP("SUNSPEC_TEST_HOST is not set");

    Thing *thing = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((thing = m_thingManager->findConfiguredThing(DeviceNoMeterThingId)) != nullptr, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(thing->setupStatus(), Thing::ThingSetupStatusComplete, 30000);

    QVERIFY2(m_plugin->testPrimaryConnection(DeviceNoMeterThingId),
             "la connexion onduleur n'a pas ete creee");
    QVERIFY2(!m_plugin->testMeterConnection(DeviceNoMeterThingId),
             "meterSlaveId a 0 ne doit ouvrir aucune connexion compteur");
}

/* The second defect invisible offline: the connected state of the parent follows
 * the primary connection alone. A Smart Meter that drops must neither mark
 * the inverter unreachable, nor erase the data it keeps delivering.
 *
 * Placed last, because it cuts a connection for good. */
void SunSpecDeviceTest::test_F3_losing_the_meter_leaves_the_inverter_alone()
{
    if (skipUnlessConfigured())
        QSKIP("SUNSPEC_TEST_HOST is not set");

    Thing *parent = m_thingManager->findConfiguredThing(DeviceThingId);
    QVERIFY(parent);
    QVERIFY2(parent->stateValue("connected").toBool(), "le parent n'etait pas connecte au depart");

    QList<Thing *> inverters = childrenOfKind(sunspecThreePhaseInverterThingClassId);
    QList<Thing *> meters = childrenOfKind(sunspecThreePhaseMeterThingClassId);
    QVERIFY2(!inverters.isEmpty(), "pas d'enfant onduleur a surveiller");
    QVERIFY2(!meters.isEmpty(), "pas d'enfant compteur a couper");
    Thing *inverterThing = inverters.first();
    Thing *meterThing = meters.first();

    SunSpecConnection *primary = m_plugin->testPrimaryConnection(DeviceThingId);
    SunSpecConnection *meter = m_plugin->testMeterConnection(DeviceThingId);
    QVERIFY(primary && meter);

    SunSpecModel *inverterModel = modelOn(primary, 113);
    SunSpecModel *meterModel = modelOn(meter, 213);
    QVERIFY2(inverterModel, "modele onduleur 113 introuvable sur la connexion primaire");
    QVERIFY2(meterModel, "modele compteur 213 introuvable sur la connexion compteur");

    /* One explicit polling round. This setup replaces the plugin's timer
     * with an empty object - we do not want to poll a device in a loop - yet
     * it is the block update that sets a child to connected. Without this round,
     * the children would never have been connected and the cut below would
     * prove nothing. */
    QSignalSpy inverterFirst(inverterModel, &SunSpecModel::blockUpdated);
    QSignalSpy meterFirst(meterModel, &SunSpecModel::blockUpdated);
    inverterModel->readBlockData();
    meterModel->readBlockData();
    QVERIFY2(inverterFirst.wait(30000), "pas de donnee onduleur avant la coupure");
    if (meterFirst.isEmpty())
        QVERIFY2(meterFirst.wait(30000), "pas de donnee compteur avant la coupure");

    QVERIFY2(inverterThing->stateValue("connected").toBool(),
             "l'enfant onduleur n'etait pas connecte avant la coupure");
    QVERIFY2(meterThing->stateValue("connected").toBool(),
             "l'enfant compteur n'etait pas connecte avant la coupure");

    // Clean cut of the meter connection only.
    meter->disconnectDevice();
    QTRY_VERIFY_WITH_TIMEOUT(!meter->connected(), 15000);

    // The parent stays connected: the primary is authoritative, never a logical AND.
    QVERIFY2(parent->stateValue("connected").toBool(),
             "le parent est passe deconnecte alors que seule la connexion compteur est tombee");
    QVERIFY(primary->connected());

    // The inverter child is not affected...
    QVERIFY2(inverterThing->stateValue("connected").toBool(),
             "l'enfant onduleur a ete marque deconnecte par la chute du compteur");
    QVERIFY2(inverterThing->stateValue("currentPower").isValid(),
             "les donnees onduleur ont ete effacees");

    // ...and its data keeps arriving.
    QSignalSpy inverterUpdated(inverterModel, &SunSpecModel::blockUpdated);
    inverterModel->readBlockData();
    QVERIFY2(inverterUpdated.wait(30000),
             "plus aucune donnee onduleur apres la chute du compteur");

    // Only the meter child is marked disconnected.
    QVERIFY2(!meterThing->stateValue("connected").toBool(),
             "l'enfant compteur aurait du etre marque deconnecte");
}

QTEST_MAIN(SunSpecDeviceTest)
#include "tst_sunspecdevice.moc"
