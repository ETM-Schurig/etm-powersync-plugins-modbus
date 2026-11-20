// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2013 - 2024, nymea GmbH
* Copyright (C) 2024 - 2025, chargebyte austria GmbH
*
* This file is part of nymea-plugins-modbus.
*
* nymea-plugins-modbus is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* nymea-plugins-modbus is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with nymea-plugins-modbus. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINSUNSPEC_H
#define INTEGRATIONPLUGINSUNSPEC_H

#include <integrations/integrationplugin.h>
#include <network/networkdevicemonitor.h>
#include <plugintimer.h>

#include <sunspecconnection.h>
#include <models/sunspecmodelfactory.h>
#include <models/sunspecmpptmodel.h>
#include <models/sunspecnameplatemodel.h>

#include "sunspecthing.h"
#include "sunspecstoragelogic.h"
#include "froniusrules.h"
#include "sunspecduplicates.h"
#include "extern-plugininfo.h"

#include <QUuid>

class IntegrationPluginSunSpec: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsunspec.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginSunSpec();

    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

#ifdef SUNSPEC_PLUGIN_TEST_ACCESS
    /* Test only, compiled in by sunspec/tests/ and by nothing else.
     *
     * Whether a thing ended up with one connection or two is not observable
     * from the outside: both connections sit behind the same network monitor
     * and the same host and port. Without this, the multi unit ID behaviour
     * could only be checked against a real device. */
    SunSpecConnection *testPrimaryConnection(const ThingId &parentId) const { return m_sunSpecConnections.value(parentId); }
    SunSpecConnection *testMeterConnection(const ThingId &parentId) const { return m_meterConnections.value(parentId); }
#endif

private:
    // SunSpec Connection params map
    QHash<ThingClassId, ParamTypeId> m_connectionDiscoverySlaveIdParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_connectionMacAddressParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_connectionAddressParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_connectionHostNameParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_connectionPortParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_connectionSlaveIdParamTypeIds;

    // SunSpec thing params map
    QHash<ThingClassId, ParamTypeId> m_modelIdParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_modbusAddressParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_manufacturerParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_deviceModelParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_serialNumberParamTypeIds;

    PluginTimer *m_refreshTimer = nullptr;

    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;

    /* The primary connection of a thing: the one that carries its identity, its
     * connected state and its timeout and retry settings. Every vendor has
     * exactly one. */
    QHash<ThingId, SunSpecConnection *> m_sunSpecConnections;

    /* Second, optional connection towards the Smart Meter of a Fronius GEN24,
     * on the same host and port as the primary one but on another unit ID.
     * Indexed on the same parent ThingId.
     *
     * Kept apart from m_sunSpecConnections rather than turning that one into a
     * hash of lists: this way not a single call site of the generic and
     * solarEdge vendors changes, including the reverse lookup
     * m_sunSpecConnections.key() the SolarEdge battery search relies on. */
    QHash<ThingId, SunSpecConnection *> m_meterConnections;

    QHash<Thing *, SunSpecThing *> m_sunSpecThings;

    QHash<Thing *, SunSpecModel *> m_sunSpecInverters;
    QHash<Thing *, SunSpecModel *> m_sunSpecMeters;
    QHash<Thing *, SunSpecModel *> m_sunSpecStorages;

    /* Companion models of a storage thing, on the same connection as its model
     * 124. They are not things of their own and never will be: they describe
     * the same battery, from the two registers model 124 does not carry.
     *
     *  - model 160 gives the instantaneous power, through its StCha /
     *    StDisCha blocks. Without it the `currentPower` of the energystorage
     *    interface has no source at all;
     *  - model 120 gives the rated capacity in WHRtg.
     *
     * Both may legitimately be absent, and a storage thing without them keeps
     * working exactly as it did before - minus the two states. */
    QHash<Thing *, SunSpecMpptModel *> m_storageMpptModels;
    QHash<Thing *, SunSpecNameplateModel *> m_storageNameplateModels;

    /* Storage things already warned about a missing model 160. The warning has
     * to be said once and not on every poll, but it does have to be said: an
     * energystorage whose currentPower can never move is exactly the silent
     * failure this whole change exists to remove. */
    QSet<ThingId> m_storagesWarnedWithoutPowerSource;

    /* Connections having completed at least one successful discovery. The
     * orphan sweep needs to know whether it is looking at the whole picture or
     * at half of it. Entries are removed when the connection is destroyed. */
    QSet<SunSpecConnection *> m_discoveredConnections;


    Thing *getThingForSunSpecModel(uint modelId, uint modbusAddress, const ThingId &parentId);
    bool sunspecThingAlreadyAdded(uint modelId, uint modbusAddress, const ThingId &parentId);
    void processDiscoveryResult(Thing *thing, SunSpecConnection *connection);

    /* Whether the children of a parent may be confronted with the models found
     * so far. False while a connection of that parent has not reported yet, so
     * that one connection does not sweep away the children of the other.
     *
     * INFERRED: no measurement backs this grace period. See D-2 in
     * DIVERGENCES.md for how to replace it with one. */
    bool orphanSweepAllowed(Thing *thing);
    QHash<ThingId, QDateTime> m_orphanSweepDeadlines;

    // SunSpec things
    SunSpecConnection *createConnection(Thing *thing, uint slaveId, bool isPrimary);

    /* Every connection of a parent thing, primary first. One entry for the
     * generic and solarEdge vendors, one or two for Fronius. */
    QList<SunSpecConnection *> connectionsFor(const ThingId &parentId) const;
    QList<SunSpecConnection *> allConnections() const;

    /* The connection a child thing reads its data from, or null while it is not
     * bound to a model yet. Resolved from the model the child is bound to
     * rather than from a persisted unit ID: a child stores only its model ID
     * and start register, and the two connections of a Fronius thing never
     * announce the same model. */
    SunSpecConnection *connectionForChild(Thing *child) const;

    /* The model a child thing is described by, looked up across every
     * connection of its parent. */
    SunSpecModel *findModelFor(const ThingId &parentId, uint modelId, int modbusStartRegister) const;

    /* Children of a parent that read from one given connection. A child not
     * bound to any model yet follows the primary connection, which is what
     * makes the parent reachable at all. */
    QList<Thing *> childThingsOn(Thing *parent, SunSpecConnection *connection) const;

    // Custom types
    void setupSolarEdgeBattery(ThingSetupInfo *info);

    // Discover SolarEdge batteries
    void searchSolarEdgeBatteries(SunSpecConnection *connection);
    void searchSolarEdgeBattery(SunSpecConnection *connection, const ThingId &parentThingId, quint16 startRegister);

    // SolarEdge
    double calculateSolarEdgePvProduction(Thing *thing, double acPower, double dcPower);

    void autocreateSunSpecModelThing(const ThingClassId &thingClassId, const QString &thingName, Thing *parent, SunSpecModel *model);
    SunSpecDuplicates::ModelFamily modelFamilyOf(Thing *thing) const;

    QString getInverterStateString(quint16 state);
    QString getInverterErrorString(quint32 flag);

    double fixValueSign(double targetValue, double powerValue);
    bool hasManufacturer(const QStringList &manufacturers, const QString &manufacturer);
    void markThingStatesDisconnected(Thing *thing);

    // Fronius
    bool isFroniusChild(const ThingId &parentId);
    int effectiveUpdateInterval();

    /* Creates the Smart Meter connection of a Fronius thing, or returns null
     * and says why. The object is created here but left idle: it is connected
     * only once the primary connection is done discovering, so that the two
     * never handshake at the same time towards the same inverter. */
    SunSpecConnection *createMeterConnection(Thing *thing);
    void startMeterConnection(Thing *thing);

    /* Destroys every connection of a thing. A single entry point, so that the
     * meter connection cannot be forgotten on the paths that used to have only
     * one connection to dispose of. */
    void destroyConnections(const ThingId &thingId);

    // Storage setpoints
    void executeSetpointPlan(ThingActionInfo *info, SunSpecStorageModel *storage, const SunSpecStorageLogic::SetpointPlan &plan);
    void executeSetpointWrite(ThingActionInfo *info, SunSpecStorageModel *storage, const SunSpecStorageLogic::SetpointPlan &plan, int index);
    QModbusReply *writeSetpointRegister(SunSpecStorageModel *storage, const SunSpecStorageLogic::Write &write);
    double readSetpointRegister(SunSpecStorageModel *storage, SunSpecStorageLogic::Register reg);
    void refreshStorageBlock(ThingActionInfo *info, SunSpecStorageModel *storage, const std::function<void()> &continuation);
    void verifySetpointPlan(ThingActionInfo *info, SunSpecStorageModel *storage, const SunSpecStorageLogic::SetpointPlan &plan);

    /* Binds a storage thing to the model 160 and model 120 of its own
     * connection, and starts publishing currentPower and capacity from them.
     * Idempotent: called from both paths that bind a storage thing to its model
     * 124, and from either order. */
    void bindStorageCompanionModels(Thing *thing, SunSpecModel *storageModel);

    /* The model 160 blocks of a storage thing, in the shape the pure rules
     * take. Empty when the thing has no model 160 bound. */
    QList<FroniusRules::ModuleReading> storageModuleReadings(Thing *thing) const;

    void updateStorageCurrentPower(Thing *thing);
    void updateStorageEnergy(Thing *thing, const StateTypeId &stateTypeId, double energyKwh);
    void updateStorageCapacity(Thing *thing);

private slots:
    void onRefreshTimer();
    void onPluginConfigurationChanged(const ParamTypeId &paramTypeId, const QVariant &value);

    void onInverterBlockUpdated();
    void onMeterBlockUpdated();
    void onStorageBlockUpdated();
    void onStorageMpptBlockUpdated();
    void onStorageNameplateBlockUpdated();
    void onSolarEdgeBatteryBlockUpdated();

    void evaluateEnergyProducedValue(Thing *inverterThing, float energyProduced);


};
#endif // INTEGRATIONPLUGINSUNSPEC_H

