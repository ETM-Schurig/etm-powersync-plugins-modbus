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

#include <QtTest>
#include <QCoreApplication>
#include <QModbusTcpServer>
#include <QDataStream>

#include <nymeasettings.h>
#include <hardwaremanager.h>
#include <plugintimer.h>
#include <logging/logengine.h>
#include <network/networkdevicediscovery.h>
#include <network/networkdevicemonitor.h>
#include <integrations/thingmanagerimplementation.h>
#include <integrations/thingsetupinfo.h>
#include <integrations/thingactioninfo.h>

#include <sunspecconnection.h>

#include "integrationpluginsunspec.h"
#include "froniusrules.h"

/* The real plugin, behind a real ThingManager, against SunSpec Modbus TCP
 * servers set up in this process on 127.0.0.1. Nothing but the network is
 * simulated: discovery, reading of the models, creation of the child
 * things and publication of the states are those of the shipped plugin.
 *
 * The revert timer (revertTime) of a Fronius storage is seen end to
 * end: the value received at creation of the child, on first installation
 * as after a restart, and the refusal of a setpoint when this timer is
 * disabled. What ../logic does not see: the discovery path during
 * setupThing(), and the writes that go out on the wire.
 *
 * The register maps are built here from the point offsets of
 * libnymea-sunspec (generated model, ID at 0). The base address 40000 is the one
 * that SunSpec discovery probes first; no other address is
 * hard-coded, each model follows the previous one. */

/* -------------------------------------------------------------------------- */
/* Platform test doubles                                                       */

class LoopbackMonitor : public NetworkDeviceMonitor
{
public:
    explicit LoopbackMonitor(QObject *parent = nullptr) : NetworkDeviceMonitor(parent) {}

    MacAddress macAddress() const override { return MacAddress(); }
    QString hostName() const override { return QString(); }
    QHostAddress address() const override { return QHostAddress(QHostAddress::LocalHost); }
    NetworkDeviceInfo::MonitorMode monitorMode() const override { return NetworkDeviceInfo::MonitorModeIp; }
    NetworkDeviceInfo networkDeviceInfo() const override { return NetworkDeviceInfo(QHostAddress(QHostAddress::LocalHost)); }
    bool reachable() const override { return true; }
    QDateTime lastSeen() const override { return QDateTime::currentDateTime(); }
    uint pingRetries() const override { return 0; }
    void setPingRetries(uint) override {}
};

class LoopbackDeviceDiscovery : public NetworkDeviceDiscovery
{
public:
    explicit LoopbackDeviceDiscovery(QObject *parent = nullptr) : NetworkDeviceDiscovery(parent) {}

    NetworkDeviceDiscoveryReply *discover() override { return nullptr; }
    bool running() const override { return false; }

    NetworkDeviceMonitor *registerMonitor(Thing *) override { return new LoopbackMonitor(this); }
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
};

/* The things created by addConfiguredThing() and the children announced by the
 * plugin log the states their thingClass suggests: a ThingManager
 * without a LogEngine dies on the first one. This one accepts all, keeps nothing. */
class NullLogEngine : public LogEngine
{
public:
    explicit NullLogEngine(QObject *parent = nullptr) : LogEngine(parent) {}

    Logger *registerLogSource(const QString &name, const QStringList &tags, Types::LoggingType loggingType, const QString &) override
    {
        if (!m_loggers.contains(name))
            m_loggers.insert(name, createLogger(name, tags, loggingType));
        return m_loggers.value(name);
    }
    void unregisterLogSource(const QString &name) override { delete m_loggers.take(name); }

    LogFetchJob *fetchLogEntries(const QStringList &, const QStringList &, const QDateTime &, const QDateTime &, const QVariantMap &,
                                 Types::SampleRate, Qt::SortOrder, int, int) override
    {
        return new LogFetchJob(this);
    }

    bool jobsRunning() const override { return false; }
    void clear(const QString &) override {}
    void enable() override {}
    void disable() override {}

private:
    void logEvent(Logger *, const QStringList &, const QVariantMap &) override {}

    QHash<QString, Logger *> m_loggers;
};

/* The periodic refresh is triggered by the test, never by the
 * clock: each read cycle is an explicit step. */
class ManualPluginTimer : public PluginTimer
{
public:
    explicit ManualPluginTimer(int seconds, QObject *parent = nullptr)
        : PluginTimer(parent), m_interval(seconds) {}

    int interval() const override { return m_interval; }
    int currentTick() const override { return 0; }
    bool running() const override { return true; }

    void reset() override {}
    void start() override {}
    void stop() override {}
    void pause() override {}
    void resume() override {}

    void fire() { emit timeout(); }

private:
    int m_interval = 0;
};

class ManualPluginTimerManager : public PluginTimerManager
{
public:
    explicit ManualPluginTimerManager(QObject *parent = nullptr) : PluginTimerManager(parent) {}

    PluginTimer *registerTimer(int seconds) override
    {
        ManualPluginTimer *timer = new ManualPluginTimer(seconds, this);
        m_timers.append(timer);
        return timer;
    }

    void unregisterTimer(PluginTimer *timer) override
    {
        m_timers.removeAll(static_cast<ManualPluginTimer *>(timer));
        delete timer;
    }

    void fireAll()
    {
        foreach (ManualPluginTimer *timer, m_timers)
            timer->fire();
    }

    bool available() const override { return true; }
    bool enabled() const override { return true; }

protected:
    void setEnabled(bool) override {}

private:
    QList<ManualPluginTimer *> m_timers;
};

class LoopbackHardwareManager : public HardwareManager
{
public:
    explicit LoopbackHardwareManager(QObject *parent = nullptr)
        : HardwareManager(parent)
        , m_pluginTimerManager(new ManualPluginTimerManager(this))
        , m_networkDeviceDiscovery(new LoopbackDeviceDiscovery(this))
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

    ManualPluginTimerManager *timers() const { return m_pluginTimerManager; }

private:
    ManualPluginTimerManager *m_pluginTimerManager = nullptr;
    LoopbackDeviceDiscovery *m_networkDeviceDiscovery = nullptr;
};

class TestableSunSpecPlugin : public IntegrationPluginSunSpec
{
public:
    using IntegrationPlugin::setMetaData;
};

/* -------------------------------------------------------------------------- */
/* Register map of a hybrid inverter                                           */

struct BatteryBlockEnergy
{
    quint32 charged = 0;      // raw DCWH of the StCha block
    quint32 discharged = 0;   // raw DCWH of the StDisCha block
};

struct DeviceImage
{
    QString serialNumber;

    /* A second inverter model behind the same Common block, further along the
     * chain: the original case of the duplicate guard. */
    bool repeatedInverter = false;

    /* Blocks of model 160: the two PV strings, then StCha / StDisCha when
     * a battery is wired. */
    bool batteryBlocks = false;

    /* Model 124. Present without a battery on a hybrid, hence the zero
     * voltages of the « no battery » case. */
    bool storageModel = false;
    quint16 storCtlMod = 0;
    quint16 inBatV = 0;
    quint16 storAval = 0;

    qint16 dcwhScaleFactor = 0;
    BatteryBlockEnergy energy;
};

static void appendString(QVector<quint16> &registers, int offset, int size, const QString &text)
{
    QByteArray bytes = text.toLatin1().left(size * 2);
    bytes.append(QByteArray(size * 2 - bytes.size(), '\0'));
    for (int i = 0; i < size; i++)
        registers[offset + i] = static_cast<quint16>((static_cast<quint8>(bytes.at(2 * i)) << 8) | static_cast<quint8>(bytes.at(2 * i + 1)));
}

/* Model 1, L = 66. Offsets of SunSpecCommonModel. */
static QVector<quint16> commonModel(const QString &serialNumber)
{
    QVector<quint16> r(68, 0);
    r[0] = 1;
    r[1] = 66;
    appendString(r, 2, 16, "Fronius");
    appendString(r, 18, 16, "Test Hybrid");
    appendString(r, 42, 8, "1.0.0");
    appendString(r, 50, 16, serialNumber);
    r[66] = 1;
    return r;
}

/* Model 103, L = 50. The values are not looked at by these tests. */
static QVector<quint16> threePhaseInverterModel()
{
    QVector<quint16> r(52, 0);
    r[0] = 103;
    r[1] = 50;
    return r;
}

/* Model 160, L = 8 + 20 N. Fixed block: DCA_SF, DCV_SF, DCW_SF, DCWH_SF, Evt, N,
 * TmsPer. Repeating block: ID, IDStr (8), DCA, DCV, DCW, DCWH (acc32), Tms, Tmp,
 * DCSt, DCEvt. High-order word first, as SunSpec specifies and as the
 * hardware emits it (D-8). */
static QVector<quint16> mpptModel(const DeviceImage &image)
{
    QStringList ids = { "MPPT 1", "MPPT 2" };
    if (image.batteryBlocks)
        ids << "StCha 3" << "StDisCha 4";

    const int blocks = ids.count();
    QVector<quint16> r(10 + 20 * blocks, 0);
    r[0] = 160;
    r[1] = static_cast<quint16>(8 + 20 * blocks);
    r[5] = static_cast<quint16>(image.dcwhScaleFactor);
    r[8] = static_cast<quint16>(blocks);

    for (int i = 0; i < blocks; i++) {
        const int b = 10 + 20 * i;
        r[b] = static_cast<quint16>(i + 1);
        appendString(r, b + 1, 8, ids.at(i));

        quint32 dcwh = 0;
        if (ids.at(i).startsWith("StCha"))
            dcwh = image.energy.charged;
        else if (ids.at(i).startsWith("StDisCha"))
            dcwh = image.energy.discharged;
        r[b + 12] = static_cast<quint16>(dcwh >> 16);
        r[b + 13] = static_cast<quint16>(dcwh & 0xFFFF);
    }
    return r;
}

/* Offsets of model 124 (ID at 0), the same as StorageBlock in ../logic. */
static const int StorCtlModOffset = 5;
static const int OutWRteOffset = 12;
static const int InWRteOffset = 13;
static const int RvrtTmsOffset = 15;
static const int ChaGriSetOffset = 17;

/* Model 124, L = 24. */
static QVector<quint16> storageModel(const DeviceImage &image)
{
    QVector<quint16> r(26, 0);
    r[0] = 124;
    r[1] = 24;
    r[2] = 5000;                        // WChaMax
    r[StorCtlModOffset] = image.storCtlMod;
    r[8] = 5000;                        // ChaState, 50.00 % with SF -2
    r[9] = image.storAval;
    r[10] = image.inBatV;
    r[11] = 6;                          // ChaSt HOLDING
    r[OutWRteOffset] = 10000;           // 100.00 %
    r[InWRteOffset] = 10000;            // 100.00 %
    r[19] = static_cast<quint16>(-2);
    r[21] = static_cast<quint16>(-2);
    r[22] = static_cast<quint16>(-2);
    r[25] = static_cast<quint16>(-2);
    return r;
}

static const quint16 SunSpecBaseRegister = 40000;
static const quint16 ImageSize = 1000;

static QVector<quint16> registersFor(const DeviceImage &image)
{
    QVector<quint16> r = { 0x5375, 0x6e53 };   // "SunS"
    r << commonModel(image.serialNumber);
    r << threePhaseInverterModel();
    r << mpptModel(image);
    if (image.storageModel)
        r << storageModel(image);
    if (image.repeatedInverter)
        r << threePhaseInverterModel();
    r << 0xFFFF << 0;                           // end of chain

    /* Always the whole range: a chain that gets shorter must not leave
     * the old end behind it. */
    r.resize(ImageSize);
    return r;
}

/* A server that keeps track of the client's writes, in their order of
 * arrival, and can refuse or ignore one. The writes of the test
 * itself (serve()) are not recorded.
 *
 * Refusing means answering with a Modbus exception: the client sees the failure.
 * Ignoring means acknowledging without changing anything: the silent failure that AC-13
 * must catch on readback. */
class RecordingModbusServer : public QModbusTcpServer
{
public:
    struct RegisterWrite
    {
        int address;
        quint16 value;
        bool accepted;
    };

    void startRecording()
    {
        m_writes.clear();
        m_recording = true;
    }
    void stopRecording()
    {
        m_recording = false;
        m_rejected = -1;
        m_ignored = -1;
    }
    void rejectWriteAt(int address) { m_rejected = address; }
    void ignoreWriteAt(int address) { m_ignored = address; }

    QList<RegisterWrite> writes() const { return m_writes; }

protected:
    bool writeData(const QModbusDataUnit &unit) override
    {
        if (!m_recording || unit.registerType() != QModbusDataUnit::HoldingRegisters)
            return QModbusTcpServer::writeData(unit);

        const int first = unit.startAddress();
        const int last = first + int(unit.valueCount()) - 1;
        const bool rejected = m_rejected >= first && m_rejected <= last;
        for (int i = 0; i < int(unit.valueCount()); i++)
            m_writes.append({first + i, unit.value(i), !rejected});

        if (rejected)
            return false;

        if (m_ignored >= first && m_ignored <= last)
            return true;

        return QModbusTcpServer::writeData(unit);
    }

private:
    bool m_recording = false;
    int m_rejected = -1;
    int m_ignored = -1;
    QList<RegisterWrite> m_writes;
};

class SunSpecDevice
{
public:
    explicit SunSpecDevice(quint16 port) : m_port(port) {}

    bool start(const DeviceImage &image)
    {
        QModbusDataUnitMap map;
        map.insert(QModbusDataUnit::HoldingRegisters,
                   QModbusDataUnit(QModbusDataUnit::HoldingRegisters, SunSpecBaseRegister, ImageSize));
        m_server.setMap(map);
        m_server.setServerAddress(1);
        m_server.setConnectionParameter(QModbusDevice::NetworkAddressParameter, "127.0.0.1");
        m_server.setConnectionParameter(QModbusDevice::NetworkPortParameter, m_port);
        if (!m_server.connectDevice())
            return false;
        serve(image);
        return true;
    }

    void serve(const DeviceImage &image)
    {
        m_image = image;
        m_server.setData(QModbusDataUnit(QModbusDataUnit::HoldingRegisters, SunSpecBaseRegister, registersFor(image)));
    }

    DeviceImage image() const { return m_image; }
    quint16 port() const { return m_port; }

    quint16 registerAt(int address) const
    {
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, 1);
        return m_server.data(&unit) ? unit.value(0) : 0xFFFF;
    }

    RecordingModbusServer *server() { return &m_server; }

private:
    quint16 m_port;
    RecordingModbusServer m_server;
    DeviceImage m_image;
};

/* -------------------------------------------------------------------------- */

class SunSpecDiscoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_B1_first_install_without_battery_creates_no_storage();
    void test_B1_hybrid_without_battery_still_gets_no_storage();
    void test_B1_same_model_on_other_registers_is_still_refused();
    void test_B1_storage_appeared_before_a_restart_is_created_once();
    void test_B1_restart_with_storage_in_config_creates_no_duplicate();
    void test_B1_storage_appearing_on_a_running_connection_is_created_once();
    void test_B1_battery_wired_to_a_running_hybrid_is_created_once();

    void test_B2_energies_are_read_and_scaled();
    void test_B2_not_accumulated_energy_is_not_published();
    void test_B2_register_full_of_ones_is_not_published();
    void test_B2_unimplemented_scale_factor_is_not_published();
    void test_B2_decreasing_total_is_published_as_a_reset();
    void test_B2_storage_without_battery_blocks_publishes_nothing();

    void test_AC_11_discharge_block_writes_one_ordered_plan();
    void test_AC_11_discharge_release_returns_to_normal();
    void test_AC_11_discharge_block_failed_first_write_writes_nothing_else();
    void test_AC_11_discharge_block_ignored_setpoint_is_caught_by_readback();

    // These restart the ThingManager: last.
    void test_AC_10_first_install_arms_the_revert_timer();
    void test_AC_10_first_install_without_battery_blocks_arms_the_revert_timer();
    void test_AC_10_storage_created_after_a_restart_arms_the_revert_timer();
    void test_AC_10_generic_child_keeps_the_revert_timer_disabled();

    void test_AC_10_armed_storage_accepts_a_limit();
    void test_AC_10_stored_zero_survives_a_restart();
    void test_AC_10_limit_without_revert_timer_is_refused_and_writes_nothing_data();
    void test_AC_10_limit_without_revert_timer_is_refused_and_writes_nothing();
    void test_AC_10_release_without_revert_timer_is_accepted();
    void test_AC_10_discharge_release_without_revert_timer_is_accepted();
    void test_AC_10_generic_child_without_revert_timer_is_not_refused();

private:
    LoopbackHardwareManager *m_hardwareManager = nullptr;
    NullLogEngine *m_logEngine = nullptr;
    ThingManagerImplementation *m_thingManager = nullptr;
    TestableSunSpecPlugin *m_plugin = nullptr;
    PluginMetadata m_metadata;

    /* One connection per scenario, hence one device per scenario: none
     * inherits the state left by another. */
    SunSpecDevice m_restarted { 15021 };
    SunSpecDevice m_running { 15022 };
    SunSpecDevice m_repeated { 15023 };
    SunSpecDevice m_hybrid { 15024 };
    SunSpecDevice m_noBlocks { 15025 };
    SunSpecDevice m_wired { 15026 };
    SunSpecDevice m_controlled { 15027 };

    // Revert timer (AC-10).
    SunSpecDevice m_firstInstall { 15131 };
    SunSpecDevice m_laterBattery { 15132 };
    SunSpecDevice m_generic { 15133 };

    QHash<quint16, ThingId> m_connections;

    void startThingManager();
    void stopThingManager();
    void restart();
    void waitForDiscoveries();
    ThingId install(const SunSpecDevice &device, const ThingClassId &connectionClassId = froniusConnectionThingClassId);
    void rediscover(const SunSpecDevice &device);
    void refresh();
    Things children(const SunSpecDevice &device, const ThingClassId &thingClassId) const;
    Thing *storageOf(const SunSpecDevice &device) const;
    static uint revertTimeOf(Thing *storage);
    void settle();

    struct ActionResult
    {
        bool finished = false;
        // An action that does not finish never passes for a success.
        Thing::ThingError status = Thing::ThingErrorTimeout;
        QString displayMessage;
    };
    ActionResult execute(Thing *thing, const ActionTypeId &actionTypeId, const ParamTypeId &paramTypeId, const QVariant &value);
    ActionResult setDischargeBlocked(Thing *storage, bool blocked);
};

void SunSpecDiscoveryTest::startThingManager()
{
    m_thingManager = new ThingManagerImplementation(m_hardwareManager, m_logEngine, QLocale(), this);
    m_plugin = new TestableSunSpecPlugin();
    m_plugin->setMetaData(m_metadata);
    m_thingManager->registerStaticPlugin(m_plugin);
}

/* What a nymead restart does: the ThingManager saves the things
 * and their states, everything is destroyed, then a new ThingManager and a new
 * plugin restore them from the configuration. The SunSpec connections, hence
 * their models, are new. */
void SunSpecDiscoveryTest::stopThingManager()
{
    delete m_thingManager;   // also destroys the plugin, its parent
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    m_thingManager = nullptr;
    m_plugin = nullptr;
}

void SunSpecDiscoveryTest::restart()
{
    stopThingManager();
    startThingManager();
    waitForDiscoveries();
}

// All connections restored, discoveries done.
void SunSpecDiscoveryTest::waitForDiscoveries()
{
    foreach (const ThingId &id, m_connections) {
        QTRY_VERIFY_WITH_TIMEOUT(m_thingManager->findConfiguredThing(id), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(m_plugin->testPrimaryConnection(id)
                                 && !m_plugin->testPrimaryConnection(id)->models().isEmpty()
                                 && !m_plugin->testPrimaryConnection(id)->discoveryRunning(), 10000);
    }
    settle();
}

/* First installation, as from the app: initial setup, discovery, then
 * creation of the announced children. */
ThingId SunSpecDiscoveryTest::install(const SunSpecDevice &device, const ThingClassId &connectionClassId)
{
    const ThingClass thingClass = m_thingManager->findThingClass(connectionClassId);

    ParamList params;
    params << Param(thingClass.paramTypes().findByName("address").id(), "127.0.0.1");
    params << Param(thingClass.paramTypes().findByName("port").id(), device.port());
    params << Param(thingClass.paramTypes().findByName("slaveId").id(), 1);
    if (connectionClassId == froniusConnectionThingClassId) {
        // No Smart Meter: a single unit ID is used here.
        params << Param(froniusConnectionThingMeterSlaveIdParamTypeId, 0);
    }

    ThingSetupInfo *info = m_thingManager->addConfiguredThing(connectionClassId, params, "Onduleur de test");
    QSignalSpy finished(info, &ThingSetupInfo::finished);
    const ThingId id = info->thing()->id();
    [&]() {
        QVERIFY(finished.wait(10000));
        QCOMPARE(info->status(), Thing::ThingErrorNoError);
    }();

    m_connections.insert(device.port(), id);
    [&]() {
        QTRY_VERIFY_WITH_TIMEOUT(!children(device, sunspecThreePhaseInverterThingClassId).isEmpty(), 5000);
    }();
    settle();
    return id;
}

/* What the plugin does on every reconnection of the primary connection. */
void SunSpecDiscoveryTest::rediscover(const SunSpecDevice &device)
{
    SunSpecConnection *connection = m_plugin->testPrimaryConnection(m_connections.value(device.port()));
    QVERIFY(connection);
    QSignalSpy finished(connection, &SunSpecConnection::discoveryFinished);
    QVERIFY(connection->startDiscovery());
    QVERIFY(finished.wait(10000));
    QVERIFY(finished.first().first().toBool());
    settle();
}

/* One refresh cycle, and time for the replies to arrive. */
void SunSpecDiscoveryTest::refresh()
{
    m_hardwareManager->timers()->fireAll();
    settle();
}

/* The announced things go through two event queues before being
 * registered (autoThingsAppeared then ThingSetupInfo::finished, both
 * queued). Wait here until nothing moves anymore, otherwise « no duplicate »
 * would only mean « not yet ». */
void SunSpecDiscoveryTest::settle()
{
    QTest::qWait(300);
}

Things SunSpecDiscoveryTest::children(const SunSpecDevice &device, const ThingClassId &thingClassId) const
{
    Things result;
    const ThingId parentId = m_connections.value(device.port());
    foreach (Thing *thing, m_thingManager->findConfiguredThings(thingClassId)) {
        if (thing->parentId() == parentId)
            result.append(thing);
    }
    return result;
}

Thing *SunSpecDiscoveryTest::storageOf(const SunSpecDevice &device) const
{
    const Things storages = children(device, sunspecStorageThingClassId);
    return storages.count() == 1 ? storages.first() : nullptr;
}

uint SunSpecDiscoveryTest::revertTimeOf(Thing *storage)
{
    return storage->paramValue(sunspecStorageThingRevertTimeParamTypeId).toUInt();
}

/* Executes the action as the engine would, through the ThingManager, and waits
 * for it to finish. The status is read in the signal: the info is destroyed
 * right after. */
SunSpecDiscoveryTest::ActionResult SunSpecDiscoveryTest::execute(Thing *thing, const ActionTypeId &actionTypeId, const ParamTypeId &paramTypeId, const QVariant &value)
{
    Action action(actionTypeId, thing->id());
    action.setParams(ParamList() << Param(paramTypeId, value));

    QSharedPointer<ActionResult> result = QSharedPointer<ActionResult>::create();
    ThingActionInfo *info = m_thingManager->executeAction(action);
    connect(info, &ThingActionInfo::finished, this, [info, result]() {
        result->finished = true;
        result->status = info->status();
        result->displayMessage = info->displayMessage();
    });

    [&]() {
        QTRY_VERIFY_WITH_TIMEOUT(result->finished, 20000);
    }();
    return *result;
}

SunSpecDiscoveryTest::ActionResult SunSpecDiscoveryTest::setDischargeBlocked(Thing *storage, bool blocked)
{
    return execute(storage, sunspecStorageSetDischargeBlockedActionTypeId,
                   sunspecStorageSetDischargeBlockedActionDischargeBlockedParamTypeId, blocked);
}

void SunSpecDiscoveryTest::initTestCase()
{
    QCoreApplication::setOrganizationName("nymea-test-sunspec-discovery");

    NymeaSettings thingSettings(NymeaSettings::SettingsRoleThings);
    thingSettings.clear();
    NymeaSettings pluginSettings(NymeaSettings::SettingsRolePlugins);
    pluginSettings.clear();

    QFile jsonFile(PLUGIN_JSON_PATH);
    QVERIFY2(jsonFile.open(QIODevice::ReadOnly), qPrintable(QString("Cannot open %1").arg(PLUGIN_JSON_PATH)));
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    m_metadata = PluginMetadata(document.object());
    QVERIFY2(m_metadata.isValid(), "metadonnee du plugin invalide");

    /* Without a battery: two PV strings, no 124. One serial number per
     * device: the duplicate guard applies to all things of the plugin, not
     * only to the children of a single connection. */
    DeviceImage plain;
    plain.serialNumber = "SN-TEST-1";
    QVERIFY(m_restarted.start(plain));
    plain.serialNumber = "SN-TEST-2";
    QVERIFY(m_running.start(plain));
    plain.serialNumber = "SN-TEST-3";
    QVERIFY(m_repeated.start(plain));

    // Hybrid without a wired battery: 124 published, voltages at zero.
    DeviceImage hybrid;
    hybrid.serialNumber = "SN-TEST-4";
    hybrid.storageModel = true;
    QVERIFY(m_hybrid.start(hybrid));

    /* Battery present in the 124 but 160 without StCha/StDisCha blocks: the shape
     * of a SunSpec device that does not expose its battery totals. */
    DeviceImage noBlocks;
    noBlocks.serialNumber = "SN-TEST-5";
    noBlocks.storageModel = true;
    noBlocks.inBatV = 512;
    QVERIFY(m_noBlocks.start(noBlocks));

    hybrid.serialNumber = "SN-TEST-6";
    QVERIFY(m_wired.start(hybrid));

    /* The target of the action tests: a hybrid whose battery is wired
     * from installation. */
    DeviceImage controlled = hybrid;
    controlled.serialNumber = "SN-TEST-7";
    controlled.batteryBlocks = true;
    controlled.inBatV = 512;
    QVERIFY(m_controlled.start(controlled));

    // Battery wired from the first commissioning, without 160 blocks.
    DeviceImage battery;
    battery.storageModel = true;
    battery.inBatV = 512;
    battery.serialNumber = "SN-TEST-11";
    QVERIFY(m_firstInstall.start(battery));
    battery.serialNumber = "SN-TEST-13";
    QVERIFY(m_generic.start(battery));

    // No battery yet: it only appears at the restart.
    plain.serialNumber = "SN-TEST-12";
    QVERIFY(m_laterBattery.start(plain));

    m_hardwareManager = new LoopbackHardwareManager(this);
    m_logEngine = new NullLogEngine(this);
    startThingManager();
    QTRY_VERIFY_WITH_TIMEOUT(!m_thingManager->findThingClass(froniusConnectionThingClassId).id().isNull(), 2000);
}

void SunSpecDiscoveryTest::cleanupTestCase()
{
    stopThingManager();
}

/* --- B1 ------------------------------------------------------------------- */

void SunSpecDiscoveryTest::test_B1_first_install_without_battery_creates_no_storage()
{
    install(m_restarted);
    install(m_running);
    install(m_repeated);
    install(m_noBlocks);

    foreach (const SunSpecDevice *device, QList<const SunSpecDevice *>({&m_restarted, &m_running, &m_repeated})) {
        QCOMPARE(children(*device, sunspecThreePhaseInverterThingClassId).count(), 1);
        QCOMPARE(children(*device, sunspecStorageThingClassId).count(), 0);
    }

    // First installation with a battery: the path that already worked.
    QCOMPARE(children(m_noBlocks, sunspecStorageThingClassId).count(), 1);
}

/* The « no phantom battery » guard (FroniusRules::hasBattery) stays intact:
 * the 124 of a hybrid without a battery creates nothing, neither at installation nor at
 * the next discovery. */
void SunSpecDiscoveryTest::test_B1_hybrid_without_battery_still_gets_no_storage()
{
    install(m_hybrid);
    QCOMPARE(children(m_hybrid, sunspecThreePhaseInverterThingClassId).count(), 1);
    QCOMPARE(children(m_hybrid, sunspecStorageThingClassId).count(), 0);

    rediscover(m_hybrid);
    QCOMPARE(children(m_hybrid, sunspecStorageThingClassId).count(), 0);
}

/* The original case of the guard: the same model announced a second time, at
 * other registers, behind the same Common block. */
void SunSpecDiscoveryTest::test_B1_same_model_on_other_registers_is_still_refused()
{
    DeviceImage image = m_repeated.image();
    image.repeatedInverter = true;
    m_repeated.serve(image);

    rediscover(m_repeated);
    QCOMPARE(children(m_repeated, sunspecThreePhaseInverterThingClassId).count(), 1);
}

/* The defect seen in the field: a battery appearing on an already
 * installed connection. At restart, the inverter child is restored BEFORE the
 * discovery announces the 124, and the storage, which carries the serial number of
 * the inverter, was discarded as a duplicate. */
void SunSpecDiscoveryTest::test_B1_storage_appeared_before_a_restart_is_created_once()
{
    DeviceImage image = m_restarted.image();
    image.batteryBlocks = true;
    image.storageModel = true;
    image.inBatV = 512;
    image.dcwhScaleFactor = 1;
    image.energy = { 12345, 6789 };
    m_restarted.serve(image);

    restart();

    QCOMPARE(children(m_restarted, sunspecThreePhaseInverterThingClassId).count(), 1);
    QCOMPARE(children(m_restarted, sunspecStorageThingClassId).count(), 1);

    // The other scenarios have not moved.
    QCOMPARE(children(m_hybrid, sunspecStorageThingClassId).count(), 0);
    QCOMPARE(children(m_repeated, sunspecThreePhaseInverterThingClassId).count(), 1);
    QCOMPARE(children(m_noBlocks, sunspecStorageThingClassId).count(), 1);
}

void SunSpecDiscoveryTest::test_B1_restart_with_storage_in_config_creates_no_duplicate()
{
    const ThingId storageId = storageOf(m_restarted) ? storageOf(m_restarted)->id() : ThingId();
    QVERIFY(!storageId.isNull());

    restart();
    QCOMPARE(children(m_restarted, sunspecStorageThingClassId).count(), 1);
    QCOMPARE(storageOf(m_restarted)->id(), storageId);

    // One more discovery, as on a reconnection.
    rediscover(m_restarted);
    QCOMPARE(children(m_restarted, sunspecStorageThingClassId).count(), 1);
    QCOMPARE(children(m_restarted, sunspecThreePhaseInverterThingClassId).count(), 1);
}

/* The 124 appears while the connection is running. The plugin sees it only at the
 * next discovery, the one of the reconnection. */
void SunSpecDiscoveryTest::test_B1_storage_appearing_on_a_running_connection_is_created_once()
{
    DeviceImage image = m_running.image();
    image.batteryBlocks = true;
    image.storageModel = true;
    image.inBatV = 512;
    m_running.serve(image);

    rediscover(m_running);
    QCOMPARE(children(m_running, sunspecStorageThingClassId).count(), 1);

    rediscover(m_running);
    rediscover(m_running);
    QCOMPARE(children(m_running, sunspecStorageThingClassId).count(), 1);
    QCOMPARE(children(m_running, sunspecThreePhaseInverterThingClassId).count(), 1);
}

/* A hybrid installed without a battery already publishes its 124, voltages at zero. We
 * wire the battery: the 160 goes from two to four blocks, which shifts the 124
 * further along the chain (INV-1 of the spec), and its voltages become non
 * zero. At the next discovery, one storage, only one. */
void SunSpecDiscoveryTest::test_B1_battery_wired_to_a_running_hybrid_is_created_once()
{
    install(m_wired);
    QCOMPARE(children(m_wired, sunspecStorageThingClassId).count(), 0);

    DeviceImage image = m_wired.image();
    image.batteryBlocks = true;
    image.inBatV = 512;
    m_wired.serve(image);

    rediscover(m_wired);
    QCOMPARE(children(m_wired, sunspecStorageThingClassId).count(), 1);

    rediscover(m_wired);
    QCOMPARE(children(m_wired, sunspecStorageThingClassId).count(), 1);
    QCOMPARE(children(m_wired, sunspecThreePhaseInverterThingClassId).count(), 1);
}

/* --- B2 ------------------------------------------------------------------- */

/* Raw DCWH 12345 and 6789, DCWH_SF = 1: 123 450 Wh charged, 67 890 Wh
 * discharged. */
void SunSpecDiscoveryTest::test_B2_energies_are_read_and_scaled()
{
    Thing *storage = storageOf(m_restarted);
    QVERIFY(storage);

    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyConsumedStateTypeId).toDouble(), 123.45);
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyProducedStateTypeId).toDouble(), 67.89);

    // Charge and discharge each advance on their own side.
    DeviceImage image = m_restarted.image();
    image.energy.charged = 12400;
    m_restarted.serve(image);
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyConsumedStateTypeId).toDouble(), 124.0);
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyProducedStateTypeId).toDouble(), 67.89);
}

void SunSpecDiscoveryTest::test_B2_not_accumulated_energy_is_not_published()
{
    Thing *storage = storageOf(m_restarted);
    QVERIFY(storage);

    DeviceImage image = m_restarted.image();
    image.energy.discharged = 0;
    m_restarted.serve(image);
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyProducedStateTypeId).toDouble(), 67.89);
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyConsumedStateTypeId).toDouble(), 124.0);
}

void SunSpecDiscoveryTest::test_B2_register_full_of_ones_is_not_published()
{
    Thing *storage = storageOf(m_restarted);
    QVERIFY(storage);

    DeviceImage image = m_restarted.image();
    image.energy.discharged = 0xFFFFFFFF;
    m_restarted.serve(image);
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyProducedStateTypeId).toDouble(), 67.89);
}

void SunSpecDiscoveryTest::test_B2_unimplemented_scale_factor_is_not_published()
{
    Thing *storage = storageOf(m_restarted);
    QVERIFY(storage);

    DeviceImage image = m_restarted.image();
    image.dcwhScaleFactor = static_cast<qint16>(0x8000);
    image.energy = { 99999, 88888 };
    m_restarted.serve(image);
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyConsumedStateTypeId).toDouble(), 124.0);
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyProducedStateTypeId).toDouble(), 67.89);
}

/* An inverter reset: the total starts again from the bottom. It is published
 * as is, and it is nymead that treats it as a reset
 * (resynchronisation, no energy counted on this cycle). */
void SunSpecDiscoveryTest::test_B2_decreasing_total_is_published_as_a_reset()
{
    Thing *storage = storageOf(m_restarted);
    QVERIFY(storage);

    DeviceImage image = m_restarted.image();
    image.dcwhScaleFactor = 1;
    image.energy = { 5, 6789 };
    m_restarted.serve(image);
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyConsumedStateTypeId).toDouble(), 0.05);
    QVERIFY(storage->stateValue(sunspecStorageTotalEnergyConsumedStateTypeId).toDouble() >= 0);
}

void SunSpecDiscoveryTest::test_B2_storage_without_battery_blocks_publishes_nothing()
{
    Thing *storage = storageOf(m_noBlocks);
    QVERIFY(storage);

    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyConsumedStateTypeId).toDouble(), 0.0);
    QCOMPARE(storage->stateValue(sunspecStorageTotalEnergyProducedStateTypeId).toDouble(), 0.0);
}

/* --- AC-11: setDischargeBlocked action ------------------------------------- */

static int storageRegister(Thing *storage, int offset)
{
    return int(storage->paramValue(sunspecStorageThingModbusAddressParamTypeId).toUInt()) + offset;
}

static QString describe(const QList<RecordingModbusServer::RegisterWrite> &writes)
{
    QStringList parts;
    foreach (const RecordingModbusServer::RegisterWrite &write, writes)
        parts << QString("%1=%2%3").arg(write.address).arg(write.value).arg(write.accepted ? "" : " (refusee)");
    return parts.join(", ");
}

/* Block: StorCtl_Mod 2, then OutWRte 0, then the timer, in one action and
 * in this order on the wire. Nothing else is written. */
void SunSpecDiscoveryTest::test_AC_11_discharge_block_writes_one_ordered_plan()
{
    // Storage created at first installation, without rediscovery.
    install(m_controlled);
    QTRY_VERIFY_WITH_TIMEOUT(storageOf(m_controlled), 5000);

    Thing *storage = storageOf(m_controlled);
    refresh();
    QVERIFY(storage->stateValue(sunspecStorageConnectedStateTypeId).toBool());

    const quint16 revertTime = static_cast<quint16>(storage->paramValue(sunspecStorageThingRevertTimeParamTypeId).toUInt());
    QCOMPARE(int(revertTime), FroniusRules::DefaultRevertTimeSeconds);
    QCOMPARE(int(m_controlled.registerAt(storageRegister(storage, 0))), 124);

    m_controlled.server()->startRecording();
    const ActionResult result = setDischargeBlocked(storage, true);
    m_controlled.server()->stopRecording();
    QCOMPARE(result.status, Thing::ThingErrorNoError);

    const QList<RecordingModbusServer::RegisterWrite> writes = m_controlled.server()->writes();
    QVERIFY2(writes.count() == 3, qPrintable(describe(writes)));
    QCOMPARE(writes.at(0).address, storageRegister(storage, StorCtlModOffset));
    QCOMPARE(int(writes.at(0).value), 2);
    QCOMPARE(writes.at(1).address, storageRegister(storage, OutWRteOffset));
    QCOMPARE(int(writes.at(1).value), 0);
    QVERIFY2(writes.last().address == storageRegister(storage, RvrtTmsOffset),
             qPrintable("le minuteur doit fermer la sequence : " + describe(writes)));
    QCOMPARE(writes.last().value, revertTime);

    // Read back and compliant.
    QVERIFY(storage->stateValue(sunspecStorageControlHealthyStateTypeId).toBool());
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageEnableDischargingStateTypeId).toBool(), true);
    QCOMPARE(storage->stateValue(sunspecStorageDischargingRateStateTypeId).toInt(), 0);
}

/* Release: the limit first (StorCtl_Mod 0), then OutWRte 100 %, then the
 * timer. This is the NORMAL line of AC-11. */
void SunSpecDiscoveryTest::test_AC_11_discharge_release_returns_to_normal()
{
    Thing *storage = storageOf(m_controlled);
    QVERIFY(storage);
    QCOMPARE(int(m_controlled.registerAt(storageRegister(storage, StorCtlModOffset))), 2);

    m_controlled.server()->startRecording();
    const ActionResult result = setDischargeBlocked(storage, false);
    m_controlled.server()->stopRecording();
    QCOMPARE(result.status, Thing::ThingErrorNoError);

    const QList<RecordingModbusServer::RegisterWrite> writes = m_controlled.server()->writes();
    QVERIFY2(writes.count() == 3, qPrintable(describe(writes)));
    QCOMPARE(writes.at(0).address, storageRegister(storage, StorCtlModOffset));
    QCOMPARE(int(writes.at(0).value), 0);
    QCOMPARE(writes.at(1).address, storageRegister(storage, OutWRteOffset));
    QCOMPARE(int(writes.at(1).value), 10000);   // 100.00 % with InOutWRte_SF = -2
    QCOMPARE(writes.at(2).address, storageRegister(storage, RvrtTmsOffset));

    QVERIFY(storage->stateValue(sunspecStorageControlHealthyStateTypeId).toBool());
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageEnableDischargingStateTypeId).toBool(), false);
    QCOMPARE(storage->stateValue(sunspecStorageDischargingRateStateTypeId).toInt(), 100);
}

/* The first write refused by the inverter: the action fails, and neither OutWRte
 * nor the timer go out. The battery stays in automatic mode. */
void SunSpecDiscoveryTest::test_AC_11_discharge_block_failed_first_write_writes_nothing_else()
{
    Thing *storage = storageOf(m_controlled);
    QVERIFY(storage);

    // Known starting registers: limit lifted, 100 %, timer at 0.
    m_controlled.serve(m_controlled.image());
    refresh();
    QCOMPARE(int(m_controlled.registerAt(storageRegister(storage, RvrtTmsOffset))), 0);

    m_controlled.server()->startRecording();
    m_controlled.server()->rejectWriteAt(storageRegister(storage, StorCtlModOffset));
    const ActionResult result = setDischargeBlocked(storage, true);
    const QList<RecordingModbusServer::RegisterWrite> writes = m_controlled.server()->writes();
    m_controlled.server()->stopRecording();

    QCOMPARE(result.status, Thing::ThingErrorHardwareFailure);
    QVERIFY2(writes.count() == 1, qPrintable(describe(writes)));
    QCOMPARE(writes.first().address, storageRegister(storage, StorCtlModOffset));
    QVERIFY(!writes.first().accepted);

    QCOMPARE(int(m_controlled.registerAt(storageRegister(storage, StorCtlModOffset))), 0);
    QCOMPARE(int(m_controlled.registerAt(storageRegister(storage, OutWRteOffset))), 10000);
    QCOMPARE(int(m_controlled.registerAt(storageRegister(storage, RvrtTmsOffset))), 0);
}

/* The inverter acknowledges OutWRte without applying it: the sequence completes,
 * timer included, and the readback reports it through controlHealthy. */
void SunSpecDiscoveryTest::test_AC_11_discharge_block_ignored_setpoint_is_caught_by_readback()
{
    Thing *storage = storageOf(m_controlled);
    QVERIFY(storage);
    QVERIFY(storage->stateValue(sunspecStorageControlHealthyStateTypeId).toBool());

    m_controlled.server()->startRecording();
    m_controlled.server()->ignoreWriteAt(storageRegister(storage, OutWRteOffset));
    const ActionResult result = setDischargeBlocked(storage, true);
    const QList<RecordingModbusServer::RegisterWrite> writes = m_controlled.server()->writes();
    m_controlled.server()->stopRecording();

    QCOMPARE(result.status, Thing::ThingErrorNoError);
    QVERIFY2(writes.count() == 3, qPrintable(describe(writes)));
    QCOMPARE(writes.last().address, storageRegister(storage, RvrtTmsOffset));
    QCOMPARE(int(m_controlled.registerAt(storageRegister(storage, OutWRteOffset))), 10000);
    QVERIFY2(!storage->stateValue(sunspecStorageControlHealthyStateTypeId).toBool(),
             "OutWRte relu a 100 % apres une consigne a 0 : la relecture doit le signaler");

    // The same plan on an inverter that applies it: healthy again.
    QCOMPARE(setDischargeBlocked(storage, true).status, Thing::ThingErrorNoError);
    QVERIFY(storage->stateValue(sunspecStorageControlHealthyStateTypeId).toBool());
}

/* --- AC-10: revert timer of a Fronius storage ------------------------------ */

/* The original defect: the discovery of the first installation completes
 * during setupThing(), before the parent connection is registered. A
 * criterion drawn from already registered things answers « not Fronius », and the
 * storage installed together with its inverter receives 0. */
void SunSpecDiscoveryTest::test_AC_10_first_install_arms_the_revert_timer()
{
    install(m_firstInstall);

    QTRY_VERIFY_WITH_TIMEOUT(storageOf(m_firstInstall), 5000);
    QCOMPARE(revertTimeOf(storageOf(m_firstInstall)), uint(FroniusRules::DefaultRevertTimeSeconds));
}

/* The same first installation, on the device whose 160 has no battery
 * blocks: its storage, created at the very start of the suite, also receives the
 * timer. */
void SunSpecDiscoveryTest::test_AC_10_first_install_without_battery_blocks_arms_the_revert_timer()
{
    Thing *storage = storageOf(m_noBlocks);
    QVERIFY(storage);
    QCOMPARE(revertTimeOf(storage), uint(FroniusRules::DefaultRevertTimeSeconds));
}

/* The battery appears after installation and the storage is created at the
 * discovery that follows a restart, connection already registered: the path
 * that already worked must remain correct. */
void SunSpecDiscoveryTest::test_AC_10_storage_created_after_a_restart_arms_the_revert_timer()
{
    install(m_laterBattery);
    QVERIFY(!storageOf(m_laterBattery));

    DeviceImage image = m_laterBattery.image();
    image.storageModel = true;
    image.inBatV = 512;
    m_laterBattery.serve(image);
    restart();

    QTRY_VERIFY_WITH_TIMEOUT(storageOf(m_laterBattery), 5000);
    QCOMPARE(revertTimeOf(storageOf(m_laterBattery)), uint(FroniusRules::DefaultRevertTimeSeconds));
}

/* The generic vendor does not change: its storage keeps the upstream default, 0. */
void SunSpecDiscoveryTest::test_AC_10_generic_child_keeps_the_revert_timer_disabled()
{
    install(m_generic, sunspecConnectionThingClassId);

    QTRY_VERIFY_WITH_TIMEOUT(storageOf(m_generic), 5000);
    QCOMPARE(revertTimeOf(storageOf(m_generic)), 0u);
}

void SunSpecDiscoveryTest::test_AC_10_armed_storage_accepts_a_limit()
{
    Thing *storage = storageOf(m_firstInstall);
    QVERIFY(storage);
    refresh();
    QVERIFY(storage->stateValue(sunspecStorageConnectedStateTypeId).toBool());
    QCOMPARE(revertTimeOf(storage), uint(FroniusRules::DefaultRevertTimeSeconds));

    m_firstInstall.server()->startRecording();
    const ActionResult result = execute(storage, sunspecStorageEnableDischargingActionTypeId,
                                        sunspecStorageEnableDischargingActionEnableDischargingParamTypeId, true);
    m_firstInstall.server()->stopRecording();
    QCOMPARE(result.status, Thing::ThingErrorNoError);

    const QList<RecordingModbusServer::RegisterWrite> writes = m_firstInstall.server()->writes();
    QVERIFY2(writes.count() == 2, qPrintable(describe(writes)));
    QCOMPARE(writes.at(0).address, storageRegister(storage, StorCtlModOffset));
    QCOMPARE(int(writes.at(0).value), 2);
    QCOMPARE(writes.at(1).address, storageRegister(storage, RvrtTmsOffset));
    QCOMPARE(int(writes.at(1).value), FroniusRules::DefaultRevertTimeSeconds);

    // Back to the starting state for the following tests.
    m_firstInstall.serve(m_firstInstall.image());
}

/* A storage already registered at 0 - created before this fix - keeps its
 * parameters: they are stored, and nothing rewrites them at startup. This is
 * what forces repairing it by hand, and what makes the refusal below
 * necessary. */
void SunSpecDiscoveryTest::test_AC_10_stored_zero_survives_a_restart()
{
    const ThingId storageId = storageOf(m_firstInstall)->id();
    stopThingManager();

    NymeaSettings settings(NymeaSettings::SettingsRoleThings);
    settings.beginGroup("ThingConfig");
    settings.beginGroup(storageId.toString());
    settings.beginGroup("Params");
    settings.setValue(sunspecStorageThingRevertTimeParamTypeId.toString(), 0);
    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.sync();

    startThingManager();
    waitForDiscoveries();

    Thing *storage = storageOf(m_firstInstall);
    QVERIFY(storage);
    QCOMPARE(storage->id(), storageId);
    QCOMPARE(revertTimeOf(storage), 0u);

    refresh();
    QVERIFY(storage->stateValue(sunspecStorageConnectedStateTypeId).toBool());
}

/* Any setpoint that sets a limit is refused when the timer is
 * disabled, before any write at all: the limit would outlive the stopping of the
 * HEMS. */
void SunSpecDiscoveryTest::test_AC_10_limit_without_revert_timer_is_refused_and_writes_nothing_data()
{
    QTest::addColumn<QUuid>("actionTypeId");
    QTest::addColumn<QUuid>("paramTypeId");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("enableDischarging true")
        << QUuid(sunspecStorageEnableDischargingActionTypeId)
        << QUuid(sunspecStorageEnableDischargingActionEnableDischargingParamTypeId) << QVariant(true);
    QTest::newRow("enableCharging true")
        << QUuid(sunspecStorageEnableChargingActionTypeId)
        << QUuid(sunspecStorageEnableChargingActionEnableChargingParamTypeId) << QVariant(true);
    QTest::newRow("dischargingRate 0")
        << QUuid(sunspecStorageDischargingRateActionTypeId)
        << QUuid(sunspecStorageDischargingRateActionDischargingRateParamTypeId) << QVariant(0);
    QTest::newRow("chargingRate 50")
        << QUuid(sunspecStorageChargingRateActionTypeId)
        << QUuid(sunspecStorageChargingRateActionChargingRateParamTypeId) << QVariant(50);
    QTest::newRow("gridCharging true")
        << QUuid(sunspecStorageGridChargingActionTypeId)
        << QUuid(sunspecStorageGridChargingActionGridChargingParamTypeId) << QVariant(true);
    QTest::newRow("setDischargeBlocked true")
        << QUuid(sunspecStorageSetDischargeBlockedActionTypeId)
        << QUuid(sunspecStorageSetDischargeBlockedActionDischargeBlockedParamTypeId) << QVariant(true);
}

void SunSpecDiscoveryTest::test_AC_10_limit_without_revert_timer_is_refused_and_writes_nothing()
{
    QFETCH(QUuid, actionTypeId);
    QFETCH(QUuid, paramTypeId);
    QFETCH(QVariant, value);

    Thing *storage = storageOf(m_firstInstall);
    QVERIFY(storage);
    QCOMPARE(revertTimeOf(storage), 0u);

    m_firstInstall.server()->startRecording();
    const ActionResult result = execute(storage, ActionTypeId(actionTypeId), ParamTypeId(paramTypeId), value);
    m_firstInstall.server()->stopRecording();

    QCOMPARE(result.status, Thing::ThingErrorInvalidParameter);
    QVERIFY2(result.displayMessage.startsWith("revert timer disabled: "), qPrintable(result.displayMessage));

    const QList<RecordingModbusServer::RegisterWrite> writes = m_firstInstall.server()->writes();
    QVERIFY2(writes.isEmpty(), qPrintable("rien ne doit partir : " + describe(writes)));
    QCOMPARE(int(m_firstInstall.registerAt(storageRegister(storage, StorCtlModOffset))), 0);
    QCOMPARE(int(m_firstInstall.registerAt(storageRegister(storage, OutWRteOffset))), 10000);
    QCOMPARE(int(m_firstInstall.registerAt(storageRegister(storage, InWRteOffset))), 10000);
    QCOMPARE(int(m_firstInstall.registerAt(storageRegister(storage, ChaGriSetOffset))), 0);
}

/* Lifting the limit hands automatic control back to the battery: it always goes through,
 * timer or not. The limit may have been set by another client, or before
 * the timer was disabled. */
void SunSpecDiscoveryTest::test_AC_10_release_without_revert_timer_is_accepted()
{
    Thing *storage = storageOf(m_firstInstall);
    QVERIFY(storage);
    QCOMPARE(revertTimeOf(storage), 0u);

    // A discharge limit left in place, clamped to 0 %.
    DeviceImage image = m_firstInstall.image();
    image.storCtlMod = 2;
    m_firstInstall.serve(image);
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageEnableDischargingStateTypeId).toBool(), true);

    m_firstInstall.server()->startRecording();
    ActionResult result = execute(storage, sunspecStorageEnableDischargingActionTypeId,
                                  sunspecStorageEnableDischargingActionEnableDischargingParamTypeId, false);
    m_firstInstall.server()->stopRecording();
    QCOMPARE(result.status, Thing::ThingErrorNoError);

    QList<RecordingModbusServer::RegisterWrite> writes = m_firstInstall.server()->writes();
    QVERIFY2(writes.count() == 2, qPrintable(describe(writes)));
    QCOMPARE(writes.at(0).address, storageRegister(storage, StorCtlModOffset));
    QCOMPARE(int(writes.at(0).value), 0);
    QCOMPARE(writes.at(1).address, storageRegister(storage, RvrtTmsOffset));
    QCOMPARE(int(writes.at(1).value), 0);

    // The rate set back to 100 % is also a lift.
    m_firstInstall.server()->startRecording();
    result = execute(storage, sunspecStorageDischargingRateActionTypeId,
                     sunspecStorageDischargingRateActionDischargingRateParamTypeId, 100);
    m_firstInstall.server()->stopRecording();
    QCOMPARE(result.status, Thing::ThingErrorNoError);

    writes = m_firstInstall.server()->writes();
    QVERIFY2(writes.count() == 2, qPrintable(describe(writes)));
    QCOMPARE(writes.at(0).address, storageRegister(storage, OutWRteOffset));
    QCOMPARE(int(writes.at(0).value), 10000);
}

/* Unblocking discharge is the NORMAL line of AC-11: a lift, accepted
 * with the timer disabled, with its full plan and the timer as it is. */
void SunSpecDiscoveryTest::test_AC_10_discharge_release_without_revert_timer_is_accepted()
{
    Thing *storage = storageOf(m_firstInstall);
    QVERIFY(storage);
    QCOMPARE(revertTimeOf(storage), 0u);

    DeviceImage image = m_firstInstall.image();
    image.storCtlMod = 2;
    m_firstInstall.serve(image);
    refresh();
    QCOMPARE(storage->stateValue(sunspecStorageEnableDischargingStateTypeId).toBool(), true);

    m_firstInstall.server()->startRecording();
    const ActionResult result = setDischargeBlocked(storage, false);
    m_firstInstall.server()->stopRecording();
    QCOMPARE(result.status, Thing::ThingErrorNoError);

    const QList<RecordingModbusServer::RegisterWrite> writes = m_firstInstall.server()->writes();
    QVERIFY2(writes.count() == 3, qPrintable(describe(writes)));
    QCOMPARE(writes.at(0).address, storageRegister(storage, StorCtlModOffset));
    QCOMPARE(int(writes.at(0).value), 0);
    QCOMPARE(writes.at(1).address, storageRegister(storage, OutWRteOffset));
    QCOMPARE(int(writes.at(1).value), 10000);
    QCOMPARE(writes.at(2).address, storageRegister(storage, RvrtTmsOffset));
    QCOMPARE(int(writes.at(2).value), 0);
}

/* The refusal is specific to Fronius: the generic vendor, whose upstream default is
 * precisely 0, writes as before. */
void SunSpecDiscoveryTest::test_AC_10_generic_child_without_revert_timer_is_not_refused()
{
    Thing *storage = storageOf(m_generic);
    QVERIFY(storage);
    QCOMPARE(revertTimeOf(storage), 0u);
    refresh();
    QVERIFY(storage->stateValue(sunspecStorageConnectedStateTypeId).toBool());

    m_generic.server()->startRecording();
    const ActionResult result = execute(storage, sunspecStorageEnableDischargingActionTypeId,
                                        sunspecStorageEnableDischargingActionEnableDischargingParamTypeId, true);
    m_generic.server()->stopRecording();
    QCOMPARE(result.status, Thing::ThingErrorNoError);

    const QList<RecordingModbusServer::RegisterWrite> writes = m_generic.server()->writes();
    QVERIFY2(writes.count() == 2, qPrintable(describe(writes)));
    QCOMPARE(writes.at(0).address, storageRegister(storage, StorCtlModOffset));
    QCOMPARE(int(writes.at(0).value), 2);
    QCOMPARE(writes.at(1).address, storageRegister(storage, RvrtTmsOffset));
    QCOMPARE(int(writes.at(1).value), 0);
}

QTEST_MAIN(SunSpecDiscoveryTest)
#include "tst_sunspecdiscovery.moc"
