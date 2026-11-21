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

#include "integrationpluginsunspec.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>

#include <sunspecmodel.h>
#include <models/sunspecmodelfactory.h>
#include <models/sunspeccommonmodel.h>

// Inverters
#include <models/sunspecinvertersinglephasemodel.h>
#include <models/sunspecinvertersinglephasefloatmodel.h>
#include <models/sunspecinvertersplitphasemodel.h>
#include <models/sunspecinvertersplitphasefloatmodel.h>
#include <models/sunspecinverterthreephasemodel.h>
#include <models/sunspecinverterthreephasefloatmodel.h>

// Meters
#include <models/sunspecmeterthreephasemodel.h>
#include <models/sunspecmetersinglephasemodel.h>
#include <models/sunspecmetersinglephasefloatmodel.h>
#include <models/sunspecmetersplitsinglephaseabnmodel.h>
#include <models/sunspecmetersplitsinglephasefloatmodel.h>
#include <models/sunspecmeterthreephasewyeconnectmodel.h>
#include <models/sunspecmeterthreephasedeltaconnectmodel.h>
#include <models/sunspecdeltaconnectthreephaseabcmetermodel.h>

#include <models/sunspecstoragemodel.h>

#include "sunspecdiscovery.h"
#include "solaredgebattery.h"

#include <QHostAddress>

/* How long the orphan sweep waits for a connection that has not reported yet
 * before going ahead without it.
 *
 * INFERRED, no measurement backs this value: see D-2 in DIVERGENCES.md. It is
 * deliberately generous, because the two failure modes are not comparable.
 * Sweeping too late leaves a stale child thing showing as disconnected;
 * sweeping too early deletes a thing of the user. */
static const int OrphanSweepGraceSeconds = 600;

IntegrationPluginSunSpec::IntegrationPluginSunSpec()
{

}

void IntegrationPluginSunSpec::init()
{
    // SunSpec connection params
    m_connectionDiscoverySlaveIdParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionDiscoverySlaveIdParamTypeId);
    m_connectionDiscoverySlaveIdParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionDiscoverySlaveIdParamTypeId);
    m_connectionDiscoverySlaveIdParamTypeIds.insert(froniusConnectionThingClassId, froniusConnectionDiscoverySlaveIdParamTypeId);

    m_connectionMacAddressParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingMacAddressParamTypeId);
    m_connectionMacAddressParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingMacAddressParamTypeId);
    m_connectionMacAddressParamTypeIds.insert(froniusConnectionThingClassId, froniusConnectionThingMacAddressParamTypeId);

    m_connectionHostNameParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingHostNameParamTypeId);
    m_connectionHostNameParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingHostNameParamTypeId);
    m_connectionHostNameParamTypeIds.insert(froniusConnectionThingClassId, froniusConnectionThingHostNameParamTypeId);

    m_connectionAddressParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingAddressParamTypeId);
    m_connectionAddressParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingAddressParamTypeId);
    m_connectionAddressParamTypeIds.insert(froniusConnectionThingClassId, froniusConnectionThingAddressParamTypeId);

    m_connectionPortParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingPortParamTypeId);
    m_connectionPortParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingPortParamTypeId);
    m_connectionPortParamTypeIds.insert(froniusConnectionThingClassId, froniusConnectionThingPortParamTypeId);

    m_connectionSlaveIdParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingSlaveIdParamTypeId);
    m_connectionSlaveIdParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingSlaveIdParamTypeId);
    m_connectionSlaveIdParamTypeIds.insert(froniusConnectionThingClassId, froniusConnectionThingSlaveIdParamTypeId);

    // Params for sunspec things
    m_modelIdParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingModelIdParamTypeId);

    m_modbusAddressParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingModbusAddressParamTypeId);

    m_manufacturerParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingManufacturerParamTypeId);

    m_deviceModelParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingDeviceModelParamTypeId);

    m_serialNumberParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingSerialNumberParamTypeId);
}

void IntegrationPluginSunSpec::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcSunSpec()) << "Failed to discover network devices. The network device discovery is not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The discovery is not available."));
        return;
    }


    SunSpecDataPoint::ByteOrder byteOrder = SunSpecDataPoint::ByteOrderLittleEndian;
    if (info->thingClassId() == solarEdgeConnectionThingClassId)
        byteOrder = SunSpecDataPoint::ByteOrderBigEndian;
    else if (info->thingClassId() == froniusConnectionThingClassId)
        byteOrder = FroniusRules::byteOrder();

    quint16 discoverySlaveId = info->params().paramValue(m_connectionDiscoverySlaveIdParamTypeIds.value(info->thingClassId())).toUInt();

    SunSpecDiscovery *discovery = new SunSpecDiscovery(hardwareManager()->networkDeviceDiscovery(), discoverySlaveId, byteOrder, info);
    // Note: we could add here more
    connect(discovery, &SunSpecDiscovery::discoveryFinished, info, [this, info, discovery](){
        foreach (const SunSpecDiscovery::Result &result, discovery->results()) {

            // Extract the manufacturer: we pick the first manufacturer name of the first common model having a manufacturer name for now
            QString manufacturer;
            if (!result.modelManufacturers.isEmpty())
                manufacturer = result.modelManufacturers.first();

            qCDebug(dcSunSpec()) << "Found manufacturers on" << result.networkDeviceInfo << result.port;
            qCDebug(dcSunSpec()) << "Manufacturers:" << result.modelManufacturers;
            qCDebug(dcSunSpec()) << "Picking manufacturer for evaluation:" << manufacturer;

            // Filter for solar edge if we got one here
            if (info->thingClassId() == solarEdgeConnectionThingClassId) {
                if (!hasManufacturer(result.modelManufacturers, "solaredge") && !hasManufacturer(result.modelManufacturers, "solar edge")) {
                    // Solar edge...we must have the manufacturer in one common model
                    continue;
                } else {
                    manufacturer = "SolarEdge";
                }
            } else if (info->thingClassId() == froniusConnectionThingClassId) {
                if (!FroniusRules::matchesManufacturer(result.modelManufacturers)) {
                    // Only offer devices that actually announce themselves as Fronius.
                    continue;
                }
                manufacturer = "Fronius";
            } else if (info->thingClassId() == sunspecConnectionThingClassId) {
                // There are some issues regarding the sunspec implementation of kostal.
                // Full support of meter, inverter and storage will be provided in the kostal plugin which makes
                // use of the native modbus communication from kostal.
                if (hasManufacturer(result.modelManufacturers, "kostal")) {
                    qCWarning(dcSunSpec()) << "Skipping Kostal manufacturer on SunSpec. Please use the native kostal integration plugin in order to add it to the system.";
                    continue;
                }
            }

            QString title;
            if (!manufacturer.isEmpty()) {
                title = manufacturer + " ";
            }
            title.append("SunSpec connection");

            QString description;
            MacAddressInfo macInfo;
            switch (result.networkDeviceInfo.monitorMode()) {
            case NetworkDeviceInfo::MonitorModeMac:
                macInfo = result.networkDeviceInfo.macAddressInfos().constFirst();
                description = result.networkDeviceInfo.address().toString();
                if (!macInfo.vendorName().isEmpty())
                    description += " - " + result.networkDeviceInfo.macAddressInfos().constFirst().vendorName();

                break;
            case NetworkDeviceInfo::MonitorModeHostName:
                description = result.networkDeviceInfo.hostName();
                break;
            case NetworkDeviceInfo::MonitorModeIp:
                description = "Interface: " + result.networkDeviceInfo.networkInterface().name();
                break;
            }

            ThingDescriptor descriptor(info->thingClassId(), title, description);

            ParamList params;
            params << Param(m_connectionMacAddressParamTypeIds.value(info->thingClassId()), result.networkDeviceInfo.thingParamValueMacAddress());
            params << Param(m_connectionHostNameParamTypeIds.value(info->thingClassId()), result.networkDeviceInfo.thingParamValueHostName());
            params << Param(m_connectionAddressParamTypeIds.value(info->thingClassId()), result.networkDeviceInfo.thingParamValueAddress());
            params << Param(m_connectionPortParamTypeIds.value(info->thingClassId()), result.port);
            params << Param(m_connectionSlaveIdParamTypeIds.value(info->thingClassId()), result.slaveId);
            descriptor.setParams(params);

            // Check if we already have set up this device
            Thing *existingThing = myThings().findByParams(params);
            if (existingThing) {
                qCDebug(dcSunSpec()) << "This thing already exists in the system:" << result.networkDeviceInfo;
                descriptor.setThingId(existingThing->id());
            }

            info->addThingDescriptor(descriptor);
        }

        // Discovery done
        info->finish(Thing::ThingErrorNoError);
    });

    discovery->startDiscovery();
}

void IntegrationPluginSunSpec::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSunSpec()) << "Setup thing" << thing;
    qCDebug(dcSunSpec()) << thing->params();

    if (thing->thingClassId() == sunspecConnectionThingClassId ||
        thing->thingClassId() == solarEdgeConnectionThingClassId ||
        thing->thingClassId() == froniusConnectionThingClassId) {

        // Handle reconfigure
        if (m_sunSpecConnections.contains(thing->id()) || m_meterConnections.contains(thing->id())) {
            qCDebug(dcSunSpec()) << "Reconfiguring existing thing" << thing->name();
            destroyConnections(thing->id());

            if (m_monitors.contains(thing)) {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(thing);
        if (!monitor) {
            qCWarning(dcSunSpec()) << "Unable to register monitor with the given params" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("Unable to set up the connection with this configuration, please reconfigure the connection."));
            return;
        }

        m_monitors.insert(thing, monitor);

        QHostAddress address = monitor->networkDeviceInfo().address();
        if (address.isNull() && info->isInitialSetup()) {
            qCWarning(dcSunSpec()) << "Cannot set up thing. The host address is not known and this is an initial setup";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The host address is not known yet. Trying later again."));
            return;
        }

        // Clean up in case the setup gets aborted
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcSunSpec()) << "Unregister monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        uint slaveId = thing->paramValue(m_connectionSlaveIdParamTypeIds.value(thing->thingClassId())).toUInt();
        SunSpecConnection *connection = createConnection(info->thing(), slaveId, true);
        connect(info, &ThingSetupInfo::aborted, connection, &SunSpecConnection::deleteLater);

        /* A Fronius GEN24 answers for its Smart Meter on another unit ID behind
         * the same host and port. The connection is created here so that it is
         * disposed of along with the thing, but stays idle: startMeterConnection()
         * brings it up once the primary connection is done discovering.
         *
         * An absent meter never fails the setup. A GEN24 without a Smart Meter
         * is a legitimate installation, and meterSlaveId has a non zero default
         * the user did not choose. */
        SunSpecConnection *meterConnection = createMeterConnection(info->thing());
        if (meterConnection) {
            /* Unlike the primary connection above, this one takes itself out of
             * its hash on the way out, so that an aborted setup does not leave a
             * dangling pointer behind. See D-4 in DIVERGENCES.md for why the
             * primary path is left as it is. */
            connect(info, &ThingSetupInfo::aborted, meterConnection, [this, thing](){
                if (m_meterConnections.contains(thing->id()))
                    m_meterConnections.take(thing->id())->deleteLater();
            });
        }

        // If this is the first setup, we must connect successfully fo finishing the setup, otherwise we let the monitor do his work
        if (info->isInitialSetup()) {

            // Only during setup
            connect(connection, &SunSpecConnection::connectedChanged, info, [this, connection, info] (bool connected) {
                if (connected) {
                    connect(connection, &SunSpecConnection::discoveryFinished, info, [this, connection, info] (bool success) {
                        if (success) {
                            qCDebug(dcSunSpec()) << "Discovery finished successfully during setup of" << connection << ". Found SunSpec data on base register" << connection->baseRegister();
                            m_sunSpecConnections.insert(info->thing()->id(), connection);
                            info->finish(Thing::ThingErrorNoError);
                            processDiscoveryResult(info->thing(), connection);
                        } else {
                            qCWarning(dcSunSpec()) << "Discovery finished with errors during setup of" << connection;
                            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The SunSpec discovery finished with errors. Please make sure this is a SunSpec device."));
                            connection->deleteLater();
                        }
                    });

                    // Perform initial discovery, finish if a valid base register has been found
                    connection->startDiscovery();
                } else {
                    info->finish(Thing::ThingErrorHardwareNotAvailable);
                    connection->deleteLater();
                }
            });

            connect(info, &ThingSetupInfo::aborted, connection, &SunSpecConnection::deleteLater);

            if (!connection->connectDevice()) {
                qCWarning(dcSunSpec()) << "Error connecting to SunSpec device" << thing;
                info->finish(Thing::ThingErrorHardwareNotAvailable);
                connection->deleteLater();
                return;
            }

        } else {
            m_sunSpecConnections.insert(thing->id(), connection);

            // Finish the setup in any case, not the initial setup and the monitor takes care about connecting
            info->finish(Thing::ThingErrorNoError);

            if (monitor->reachable()) {
                // Thing already reachable...let's continue with the setup
                connection->connectDevice();
            } else {
                // Wait for the monitor to be ready
                qCDebug(dcSunSpec()) << "Waiting for the network monitor to get reachable before continue to set up the connection" << thing->name() << address.toString() << "...";
            }
        }

    } else if (thing->thingClassId() == sunspecThreePhaseInverterThingClassId ||
               thing->thingClassId() == sunspecSplitPhaseInverterThingClassId ||
               thing->thingClassId() == sunspecSinglePhaseInverterThingClassId ) {

        Thing *thing = info->thing();
        uint modelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toInt();
        int modbusStartRegister = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toInt();
        // Get the model from the connections of the parent if already available
        SunSpecModel *model = findModelFor(thing->parentId(), modelId, modbusStartRegister);
        if (model) {
            connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onInverterBlockUpdated);
            m_sunSpecInverters.insert(thing, model);
            qCDebug(dcSunSpec()) << "Model initialized successfully for" << thing;
        }
        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == sunspecThreePhaseMeterThingClassId
               || thing->thingClassId() == sunspecSplitPhaseMeterThingClassId
               || thing->thingClassId() == sunspecSinglePhaseMeterThingClassId ) {

        Thing *thing = info->thing();
        uint modelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toInt();
        int modbusStartRegister = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toInt();
        /* Searched across every connection of the parent: on a Fronius thing the
         * meter answers on another unit ID than the inverter. */
        SunSpecModel *model = findModelFor(thing->parentId(), modelId, modbusStartRegister);
        if (model) {
            m_sunSpecMeters.insert(thing, model);
            connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onMeterBlockUpdated);
            qCDebug(dcSunSpec()) << "Model initialized successfully for" << thing;
        } else {
            // Note: will be initialized once the connection is available and discovered.
            qCDebug(dcSunSpec()) << "Model not available yet for" << thing;
        }

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == sunspecStorageThingClassId) {
        Thing *thing = info->thing();
        uint modelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toInt();
        int modbusStartRegister = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toInt();
        SunSpecModel *model = findModelFor(thing->parentId(), modelId, modbusStartRegister);
        if (model) {
            m_sunSpecStorages.insert(thing, model);
            connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onStorageBlockUpdated);
            bindStorageCompanionModels(thing, model);
            qCDebug(dcSunSpec()) << "Model initialized successfully for" << thing;
        } else {
            // Note: will be initialized once the connection is available and discovered.
            qCDebug(dcSunSpec()) << "Model not available yet for" << thing;
        }

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == solarEdgeBatteryThingClassId) {

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing->setupStatus() == Thing::ThingSetupStatusComplete) {
            setupSolarEdgeBattery(info);
        } else {
            connect(parentThing, &Thing::setupStatusChanged, info, [this, info] {
                setupSolarEdgeBattery(info);
            });
        }
    } else {
        Q_ASSERT_X(false, "setupThing", QString("Unhandled thingClassId: %1").arg(info->thing()->thingClassId().toString()).toUtf8());
    }
}

void IntegrationPluginSunSpec::postSetupThing(Thing *thing)
{
    qCDebug(dcSunSpec()) << "Post setup thing" << thing->name();

    if (thing->thingClassId() == solarEdgeConnectionThingClassId) {
        SunSpecConnection *connection = m_sunSpecConnections.value(thing->id());
        if (connection) {
            searchSolarEdgeBatteries(connection);
        }
    }

    // Create the refresh timer if not already set up
    if (!m_refreshTimer) {
        qCDebug(dcSunSpec()) << "Starting refresh timer";
        int refreshTime = effectiveUpdateInterval();
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(refreshTime);
        connect(m_refreshTimer, &PluginTimer::timeout, this, &IntegrationPluginSunSpec::onRefreshTimer);
    }
}

void IntegrationPluginSunSpec::thingRemoved(Thing *thing)
{
    qCDebug(dcSunSpec()) << "Thing removed" << thing->name();

    /* A Fronius thing owns two connections. The chain below is an else if, so
     * without taking both here the meter one would survive its thing, keeping
     * its socket open and its signals pointing at a destroyed Thing. */
    if (m_sunSpecConnections.contains(thing->id()) || m_meterConnections.contains(thing->id())) {
        destroyConnections(thing->id());
    } else if (m_sunSpecThings.contains(thing)) {
        m_sunSpecThings.take(thing)->deleteLater();
    } else if (m_sunSpecInverters.contains(thing)) {
        m_sunSpecInverters.remove(thing);
    } else if (m_sunSpecMeters.contains(thing)) {
        m_sunSpecMeters.remove(thing);
    } else if (m_sunSpecStorages.contains(thing)) {
        m_sunSpecStorages.remove(thing);
        m_storageMpptModels.remove(thing);
        m_storageNameplateModels.remove(thing);
        m_storagesWarnedWithoutPowerSource.remove(thing->id());
    } else {
        Q_ASSERT_X(false, "thingRemoved", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
    }

    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty()) {
        qCDebug(dcSunSpec()) << "Stopping refresh timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

void IntegrationPluginSunSpec::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == sunspecStorageThingClassId) {
        SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
        if (!storage) {
            qWarning(dcSunSpec()) << "Could not find sunspec model instance for thing" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!thing->stateValue(sunspecStorageConnectedStateTypeId).toBool()) {
            qWarning(dcSunSpec()) << "Could not execute action for" << thing << "because the SunSpec connection is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The SunSpec connection is not connected."));
            return;
        }

        /* A whole battery mode rather than a single register: StorCtl_Mod and
         * OutWRte in one ordered, verified sequence. See
         * FroniusRules::planDischargeBlock(). */
        if (action.actionTypeId() == sunspecStorageSetDischargeBlockedActionTypeId) {
            const bool blocked = action.param(sunspecStorageSetDischargeBlockedActionDischargeBlockedParamTypeId).value().toBool();
            const uint revertTime = thing->paramValue(sunspecStorageThingRevertTimeParamTypeId).toUInt();
            executeSetpointPlan(info, storage, FroniusRules::planDischargeBlock(blocked, revertTime));
            return;
        }

        /* Every setpoint goes through a plan rather than a lone write. The plan
         * puts the registers in the order the hardware expects and closes the
         * sequence with the revert timer, so that a setpoint left behind by a
         * stopped HEMS expires instead of holding the battery forever. */
        QVector<SunSpecStorageLogic::Write> setpoints;

        if (action.actionTypeId() == sunspecStorageGridChargingActionTypeId) {
            bool gridCharging = action.param(sunspecStorageGridChargingActionGridChargingParamTypeId).value().toBool();
            setpoints.append({SunSpecStorageLogic::RegisterChaGriSet,
                              gridCharging ? double(SunSpecStorageModel::ChagrisetGrid) : double(SunSpecStorageModel::ChagrisetPv)});
        } else if (action.actionTypeId() == sunspecStorageEnableChargingActionTypeId) {
            SunSpecStorageModel::Storctl_modFlags controlModeFlags = storage->storCtlMod();
            bool enabled = action.param(sunspecStorageEnableChargingActionEnableChargingParamTypeId).value().toBool();
            controlModeFlags.setFlag(SunSpecStorageModel::Storctl_modCharge, enabled);
            setpoints.append({SunSpecStorageLogic::RegisterStorCtlMod, double(int(controlModeFlags))});
        } else if (action.actionTypeId() == sunspecStorageEnableDischargingActionTypeId) {
            SunSpecStorageModel::Storctl_modFlags controlModeFlags = storage->storCtlMod();
            bool enabled = action.param(sunspecStorageEnableDischargingActionEnableDischargingParamTypeId).value().toBool();
            controlModeFlags.setFlag(SunSpecStorageModel::Storctl_modDiScharge, enabled);
            setpoints.append({SunSpecStorageLogic::RegisterStorCtlMod, double(int(controlModeFlags))});
        } else if (action.actionTypeId() == sunspecStorageChargingRateActionTypeId) {
            setpoints.append({SunSpecStorageLogic::RegisterInWRte,
                              action.param(sunspecStorageChargingRateActionChargingRateParamTypeId).value().toDouble()});
        } else if (action.actionTypeId() == sunspecStorageDischargingRateActionTypeId) {
            setpoints.append({SunSpecStorageLogic::RegisterOutWRte,
                              action.param(sunspecStorageDischargingRateActionDischargingRateParamTypeId).value().toDouble()});
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(action.actionTypeId().toString()).toUtf8());
            info->finish(Thing::ThingErrorActionTypeNotFound);
            return;
        }

        const uint revertTime = thing->paramValue(sunspecStorageThingRevertTimeParamTypeId).toUInt();
        executeSetpointPlan(info, storage, SunSpecStorageLogic::planSetpoint(setpoints, revertTime));

    } else {
        Q_ASSERT_X(false, "executeAction", QString("Unhandled thingClassId: %1").arg(info->thing()->thingClassId().toString()).toUtf8());
    }
}

/* The refresh timer is plugin wide, not per connection, so the Fronius polling
 * floor has to be applied to it: as soon as one Fronius connection is
 * configured, the whole plugin stops polling faster than the floor.
 *
 * Going below it can push the inverter offline towards the manufacturer's
 * portal, and recovery then needs an AC+DC power cycle on site. This is a guard
 * in the code on purpose, not a configurable default - including in debug. */
int IntegrationPluginSunSpec::effectiveUpdateInterval()
{
    const int configured = configValue(sunSpecPluginUpdateIntervalParamTypeId).toInt();

    if (myThings().filterByThingClassId(froniusConnectionThingClassId).isEmpty())
        return configured;

    const int clamped = FroniusRules::clampUpdateInterval(configured);
    if (clamped != configured) {
        qCWarning(dcSunSpec()) << "Update interval of" << configured << "s is below the"
                               << FroniusRules::MinimumUpdateIntervalSeconds
                               << "s floor required while a Fronius connection is configured. Using"
                               << clamped << "s instead.";
    }
    return clamped;
}

bool IntegrationPluginSunSpec::isFroniusChild(const ThingId &parentId)
{
    Thing *parent = myThings().findById(parentId);
    return parent && parent->thingClassId() == froniusConnectionThingClassId;
}

QModbusReply *IntegrationPluginSunSpec::writeSetpointRegister(SunSpecStorageModel *storage, const SunSpecStorageLogic::Write &write)
{
    switch (write.reg) {
    case SunSpecStorageLogic::RegisterChaGriSet:
        return storage->setChaGriSet(write.value != 0 ? SunSpecStorageModel::ChagrisetGrid
                                                      : SunSpecStorageModel::ChagrisetPv);
    case SunSpecStorageLogic::RegisterStorCtlMod:
        return storage->setStorCtlMod(SunSpecStorageModel::Storctl_modFlags(int(write.value)));
    case SunSpecStorageLogic::RegisterInWRte:
        return storage->setInWRte(write.value);
    case SunSpecStorageLogic::RegisterOutWRte:
        return storage->setOutWRte(write.value);
    case SunSpecStorageLogic::RegisterRvrtTms:
        return storage->setInOutWRteRvrtTms(static_cast<quint16>(write.value));
    }
    return nullptr;
}

/* The writes are issued one after another rather than all at once: the order is
 * part of the contract with the hardware, and several inverters ask for
 * sequential requests. */
void IntegrationPluginSunSpec::executeSetpointWrite(ThingActionInfo *info, SunSpecStorageModel *storage, const SunSpecStorageLogic::SetpointPlan &plan, int index)
{
    if (index >= plan.writes.count()) {
        if (plan.verifyReadback.isEmpty()) {
            info->finish(Thing::ThingErrorNoError);
            return;
        }

        refreshStorageBlock(info, storage, [this, info, storage, plan]() {
            verifySetpointPlan(info, storage, plan);
        });
        return;
    }

    const SunSpecStorageLogic::Write write = plan.writes.at(index);

    /* Defence in depth: a Fronius inverter accepts some writes and then ignores
     * them without raising a Modbus exception, so the failure is invisible.
     * Those registers are refused before anything goes on the wire rather than
     * silently pretending to have worked. */
    if (isFroniusChild(info->thing()->parentId())
        && FroniusRules::isWriteForbidden(SunSpecStorageLogic::registerName(write.reg))) {
        qCWarning(dcSunSpec()) << "Refusing to write" << SunSpecStorageLogic::registerName(write.reg)
                               << "on" << info->thing()
                               << "- this register is ignored by the inverter without raising an exception.";
        info->finish(Thing::ThingErrorHardwareFailure);
        return;
    }

    QModbusReply *reply = writeSetpointRegister(storage, write);
    if (!reply) {
        qCWarning(dcSunSpec()) << "Could not write" << SunSpecStorageLogic::registerName(write.reg) << "on" << info->thing();
        info->finish(Thing::ThingErrorHardwareFailure);
        return;
    }

    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, info, [this, info, storage, plan, index, write, reply] {
        if (reply->error() != QModbusDevice::NoError) {
            qCWarning(dcSunSpec()) << "Error writing" << SunSpecStorageLogic::registerName(write.reg)
                                   << "on" << info->thing() << reply->errorString();
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        executeSetpointWrite(info, storage, plan, index + 1);
    });
}

double IntegrationPluginSunSpec::readSetpointRegister(SunSpecStorageModel *storage, SunSpecStorageLogic::Register reg)
{
    switch (reg) {
    case SunSpecStorageLogic::RegisterChaGriSet:
        return storage->chaGriSet();
    case SunSpecStorageLogic::RegisterStorCtlMod:
        return int(storage->storCtlMod());
    case SunSpecStorageLogic::RegisterInWRte:
        return storage->inWRte();
    case SunSpecStorageLogic::RegisterOutWRte:
        return storage->outWRte();
    case SunSpecStorageLogic::RegisterRvrtTms:
        return storage->inOutWRteRvrtTms();
    }
    return 0;
}

/* Reads the model block again and calls the continuation once the fresh values
 * are in. The guard timer keeps a lost or unanswered read from stranding the
 * action forever; the continuation then runs on stale values, which is why it
 * warns. */
void IntegrationPluginSunSpec::refreshStorageBlock(ThingActionInfo *info, SunSpecStorageModel *storage, const std::function<void()> &continuation)
{
    QObject *context = new QObject(info);
    QSharedPointer<bool> fired = QSharedPointer<bool>::create(false);

    auto proceed = [context, fired, continuation](bool timedOut) {
        if (*fired)
            return;
        *fired = true;
        if (timedOut)
            qCWarning(dcSunSpec()) << "Timed out waiting for a fresh storage block; continuing on the previous values.";
        context->deleteLater();
        continuation();
    };

    connect(storage, &SunSpecModel::blockUpdated, context, [proceed]() { proceed(false); });

    QTimer *guard = new QTimer(context);
    guard->setSingleShot(true);
    guard->setInterval(FroniusRules::DefaultTimeoutMilliSeconds * (FroniusRules::DefaultNumberOfRetries + 1));
    connect(guard, &QTimer::timeout, context, [proceed]() { proceed(true); });
    guard->start();

    storage->readBlockData();
}

/* Write, read back, compare. An inverter may accept a write and then ignore it
 * without raising a Modbus exception - control not enabled on the device, grid
 * charging not allowed, an IP restriction, or another controller overwriting
 * the setpoints. An unverified setpoint is an unapplied setpoint. */
void IntegrationPluginSunSpec::verifySetpointPlan(ThingActionInfo *info, SunSpecStorageModel *storage, const SunSpecStorageLogic::SetpointPlan &plan)
{
    bool healthy = true;

    foreach (const SunSpecStorageLogic::Write &write, plan.writes) {
        if (!plan.verifyReadback.contains(write.reg))
            continue;

        const double readBack = readSetpointRegister(storage, write.reg);

        /* The rates carry a scale factor of -2, so anything within a hundredth
         * is the same value. The enums and the bitfield compare exactly. */
        const double tolerance = SunSpecStorageLogic::isScaledRate(write.reg) ? 0.011 : 0.0001;
        if (qAbs(readBack - write.value) > tolerance) {
            qCWarning(dcSunSpec()) << "Setpoint not applied on" << info->thing() << ":"
                                   << SunSpecStorageLogic::registerName(write.reg)
                                   << "was written as" << write.value << "but reads back as" << readBack;
            healthy = false;
        }
    }

    info->thing()->setStateValue(sunspecStorageControlHealthyStateTypeId, healthy);
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginSunSpec::executeSetpointPlan(ThingActionInfo *info, SunSpecStorageModel *storage, const SunSpecStorageLogic::SetpointPlan &plan)
{
    /* A Fronius storage set with revertTime 0 has no revert timer: a limit
     * written now would outlive a stopped HEMS. Nothing is written, not even
     * the first register. Handing control back to the battery is let through. */
    if (isFroniusChild(info->thing()->parentId())
        && FroniusRules::isRefusedWithoutRevertTimer(plan, int(storage->storCtlMod()))) {
        qCWarning(dcSunSpec()) << "Refusing a setpoint on" << info->thing()
                               << "- its revert time is 0, so the setpoint would never expire.";
        info->finish(Thing::ThingErrorInvalidParameter,
                     QT_TR_NOOP("revert timer disabled: set a revert time above 0 on this storage before limiting the battery."));
        return;
    }

    if (!plan.rereadScaleFactor) {
        executeSetpointWrite(info, storage, plan, 0);
        return;
    }

    /* Scale factors are not guaranteed static: they may change with firmware
     * and at runtime. A factor cached during an earlier poll can put the
     * setpoint an order of magnitude off, so refresh it right before writing. */
    refreshStorageBlock(info, storage, [this, info, storage, plan]() {
        executeSetpointWrite(info, storage, plan, 0);
    });
}

Thing *IntegrationPluginSunSpec::getThingForSunSpecModel(uint modelId, uint modbusAddress, const ThingId &parentId)
{
    foreach (Thing *thing, myThings()) {
        if (!m_modelIdParamTypeIds.contains(thing->thingClassId()))
            continue;

        uint thingModelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toUInt();
        uint thingModbusAddress = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toUInt();
        if (thingModelId == modelId && thingModbusAddress == modbusAddress && thing->parentId() == parentId) {
            return thing;
        }
    }

    return nullptr;
}

bool IntegrationPluginSunSpec::sunspecThingAlreadyAdded(uint modelId, uint modbusAddress, const ThingId &parentId)
{
    return getThingForSunSpecModel(modelId, modbusAddress, parentId)!= nullptr;
}

void IntegrationPluginSunSpec::processDiscoveryResult(Thing *thing, SunSpecConnection *connection)
{
    qCInfo(dcSunSpec()) << "Processing discovery result from" << thing->name() << connection;

    /* Recorded here rather than in the discoveryFinished handler: the initial
     * setup calls this function from a second handler on the same signal, so
     * doing it there would make the orphan sweep depend on the order Qt happens
     * to invoke the two. */
    m_discoveredConnections.insert(connection);

    foreach (SunSpecModel *model, connection->models()) {
        Thing *modelThing = getThingForSunSpecModel(model->modelId(), model->modbusStartRegister(), thing->id());
        if (modelThing) {

            // We already set up a thing for this model. Make sure we are actually monitoring and updating it

            qCDebug(dcSunSpec()) << "Found" << modelThing << "for" << model;

            if (modelThing->thingClassId() == sunspecSinglePhaseInverterThingClassId
                || modelThing->thingClassId() == sunspecSplitPhaseInverterThingClassId
                || modelThing->thingClassId() == sunspecThreePhaseInverterThingClassId) {

                if (!m_sunSpecInverters.contains(modelThing)) {
                    m_sunSpecInverters.insert(modelThing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onInverterBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << modelThing;
                }
            } else if (modelThing->thingClassId() == sunspecSinglePhaseMeterThingClassId
                       || modelThing->thingClassId() == sunspecSplitPhaseMeterThingClassId
                       || modelThing->thingClassId() == sunspecThreePhaseMeterThingClassId) {

                if (!m_sunSpecMeters.contains(modelThing)) {
                    m_sunSpecMeters.insert(modelThing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onMeterBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << modelThing;
                }
            } else if (modelThing->thingClassId() == sunspecStorageThingClassId) {

                if (!m_sunSpecStorages.contains(modelThing)) {
                    m_sunSpecStorages.insert(modelThing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onStorageBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << modelThing;
                }

                /* Outside the guard above: a storage thing bound to its model
                 * 124 before its connection finished discovering has no
                 * companion models yet, and would never get them if this only
                 * ran on the cycle that created the binding. */
                bindStorageCompanionModels(modelThing, model);
            }
        } else {

            // No thing for this model, let's see if we can create one

            switch (model->modelId()) {
            case SunSpecModelFactory::ModelIdCommon:
                // Skip the common model, we already handled this one for each thing model
                break;
            case SunSpecModelFactory::ModelIdInverterSinglePhase:
            case SunSpecModelFactory::ModelIdInverterSinglePhaseFloat:
                autocreateSunSpecModelThing(sunspecSinglePhaseInverterThingClassId, QT_TR_NOOP("Single Phase Inverter"), thing, model);
                break;
            case SunSpecModelFactory::ModelIdInverterSplitPhase:
            case SunSpecModelFactory::ModelIdInverterSplitPhaseFloat:
                autocreateSunSpecModelThing(sunspecSplitPhaseInverterThingClassId, QT_TR_NOOP("Split Phase Inverter"), thing, model);
                break;
            case SunSpecModelFactory::ModelIdInverterThreePhase:
            case SunSpecModelFactory::ModelIdInverterThreePhaseFloat:
                autocreateSunSpecModelThing(sunspecThreePhaseInverterThingClassId, QT_TR_NOOP("Three Phase Inverter"), thing, model);
                break;
            case SunSpecModelFactory::ModelIdMeterSinglePhase:
            case SunSpecModelFactory::ModelIdMeterSinglePhaseFloat:
                autocreateSunSpecModelThing(sunspecSinglePhaseMeterThingClassId, QT_TR_NOOP("Single Phase Meter"), thing, model);
                break;
            case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseAbn:
            case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseFloat:
                autocreateSunSpecModelThing(sunspecSplitPhaseMeterThingClassId, QT_TR_NOOP("Split Phase Meter"), thing, model);
                break;
            case SunSpecModelFactory::ModelIdMeterThreePhase:
            case SunSpecModelFactory::ModelIdDeltaConnectThreePhaseAbcMeter:
            case SunSpecModelFactory::ModelIdMeterThreePhaseWyeConnect:
            case SunSpecModelFactory::ModelIdMeterThreePhaseDeltaConnect:
                autocreateSunSpecModelThing(sunspecThreePhaseMeterThingClassId, QT_TR_NOOP("Three Phase Meter"), thing, model);
                break;
            case SunSpecModelFactory::ModelIdStorage: {
                /* A hybrid GEN24 publishes model 124 even with no battery
                 * wired to it. Creating the child anyway gives the customer a
                 * phantom battery stuck on batteryCritical. Only the Fronius
                 * path is gated; the generic and solarEdge vendors keep their
                 * previous behaviour. */
                /* Asked of the connection thing we already hold, not through
                 * isFroniusChild(): during the initial setup the parent is not
                 * registered in myThings() yet, so a lookup answers false and
                 * the gate would be skipped exactly when it is needed. */
                SunSpecStorageModel *storageModel = qobject_cast<SunSpecStorageModel *>(model);
                if (storageModel && thing->thingClassId() == froniusConnectionThingClassId
                    && !FroniusRules::hasBattery(storageModel->inBatV(), storageModel->storAval())) {
                    qCDebug(dcSunSpec()) << "Fronius model 124 found but no battery is installed"
                                         << "(InBatV" << storageModel->inBatV()
                                         << "StorAval" << storageModel->storAval()
                                         << "). Not creating a storage thing.";
                    break;
                }
                autocreateSunSpecModelThing(sunspecStorageThingClassId, QT_TR_NOOP("Storage"), thing, model);
                break;
            }
            default:
                qCWarning(dcSunSpec()) << "Plugin has no implementation for detected" << model;
                break;
            }
        }
    }

    // Check if we have a model for all child devices, otherwise we let the device dissappear
    // As of now there might be following situation where this code is required:
    //  - A setup has changed and something has been removed or replaced
    //  - Some SunSpec device seem to communicate different model id depending on the startup phase
    //    i.e. they communicate a SinglePhase Meter on register x, few minutes later it is a 3 phase meter on x
    // This code should handle such weird setups...

    if (connection->models().isEmpty())
        return;

    /* With more than one connection under the same parent, one connection only
     * ever sees half of the picture. Sweeping on that half would make the
     * children of the other half disappear on every single discovery. So the
     * sweep waits for every connection of this parent to have reported, and
     * confronts the children with the union of their models. */
    if (!orphanSweepAllowed(thing))
        return;

    QList<SunSpecModel *> knownModels;
    foreach (SunSpecConnection *parentConnection, connectionsFor(thing->id())) {
        if (m_discoveredConnections.contains(parentConnection))
            knownModels.append(parentConnection->models());
    }

    foreach (Thing *child, myThings().filterByParentId(thing->id())) {
        if (!m_modelIdParamTypeIds.contains(child->thingClassId()) || !m_modbusAddressParamTypeIds.contains(child->thingClassId()))
            continue;

        uint childModelId = child->paramValue(m_modelIdParamTypeIds.value(child->thingClassId())).toUInt();
        uint childModbusAddress = child->paramValue(m_modbusAddressParamTypeIds.value(child->thingClassId())).toUInt();

        bool modelFoundForChild = false;
        foreach (SunSpecModel *model, knownModels) {
            if (childModelId == model->modelId() && childModbusAddress == model->modbusStartRegister()) {
                modelFoundForChild = true;
                break;
            }
        }

        if (!modelFoundForChild) {
            qCInfo(dcSunSpec()) << "The model for" << child << "does not seem to be available any more on" << thing->name() << "Removing the device since it does not seem to exist ony more on this connection.";
            emit autoThingDisappeared(child->id());
        }
    }
}

bool IntegrationPluginSunSpec::orphanSweepAllowed(Thing *thing)
{
    bool everyConnectionReported = true;
    foreach (SunSpecConnection *connection, connectionsFor(thing->id())) {
        if (!m_discoveredConnections.contains(connection)) {
            everyConnectionReported = false;
            break;
        }
    }

    if (everyConnectionReported) {
        m_orphanSweepDeadlines.remove(thing->id());
        return true;
    }

    /* Hold the sweep back, but not forever: a connection whose discovery never
     * returns would otherwise freeze it for good, and a child thing left over
     * from a previous setup would never be cleaned up.
     *
     * The grace period is deliberately generous. The two outcomes are not
     * comparable: sweeping too late leaves a stale thing showing as
     * disconnected, sweeping too early deletes a thing of the user. */
    if (!m_orphanSweepDeadlines.contains(thing->id())) {
        m_orphanSweepDeadlines.insert(thing->id(), QDateTime::currentDateTime().addSecs(OrphanSweepGraceSeconds));
        qCDebug(dcSunSpec()) << "Holding the orphan sweep of" << thing->name()
                             << "back until every connection has reported, or for"
                             << OrphanSweepGraceSeconds << "s at the most.";
        return false;
    }

    if (QDateTime::currentDateTime() < m_orphanSweepDeadlines.value(thing->id()))
        return false;

    qCWarning(dcSunSpec()) << "A connection of" << thing->name() << "never finished a discovery within"
                           << OrphanSweepGraceSeconds << "s. Sweeping orphan children against the connections"
                           << "that did report.";
    return true;
}


/* Creates one SunSpec connection for a thing.
 *
 * A thing has exactly one primary connection, and a Fronius connection may have
 * a second one towards its Smart Meter on another unit ID. Everything the two
 * share - byte order, timeout, retries, rediscovery on reconnect - lives here;
 * everything that belongs to the thing as a whole rather than to one of its
 * connections is guarded by isPrimary. */
SunSpecConnection *IntegrationPluginSunSpec::createConnection(Thing *thing, uint slaveId, bool isPrimary)
{
    QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();
    int port = thing->paramValue(m_connectionPortParamTypeIds.value(thing->thingClassId())).toInt();

    QHash<ThingId, SunSpecConnection *> &connections = isPrimary ? m_sunSpecConnections : m_meterConnections;
    if (connections.contains(thing->id())) {
        qCDebug(dcSunSpec()) << "Reconfigure SunSpec connection" << thing << "on slave ID" << slaveId;
        connections.take(thing->id())->deleteLater();
    }

    SunSpecConnection *connection = nullptr;
    if (thing->thingClassId() == solarEdgeConnectionThingClassId) {
        // Note: for some reason, solar edge is using big endian register order instead
        // of little endian as specified in sunspec and even solar edge documentation
        connection = new SunSpecConnection(address, port, slaveId, SunSpecDataPoint::ByteOrderBigEndian, this);
    } else if (thing->thingClassId() == froniusConnectionThingClassId) {
        // No endianness parameter here: the byte order is imposed, not chosen.
        connection = new SunSpecConnection(address, port, slaveId, FroniusRules::byteOrder(), this);
    } else {
        SunSpecDataPoint::ByteOrder endianness = SunSpecDataPoint::ByteOrderLittleEndian;
        QString endiannessParam = thing->paramValue("endianness").toString();
        if (endiannessParam == "Big Endian")
            endianness = SunSpecDataPoint::ByteOrderBigEndian;

        connection = new SunSpecConnection(address, port, slaveId, endianness, this);
    }

    if (thing->thingClassId() == froniusConnectionThingClassId) {
        connection->setTimeout(FroniusRules::DefaultTimeoutMilliSeconds);
        connection->setNumberOfRetries(FroniusRules::DefaultNumberOfRetries);
    } else {
        connection->setTimeout(configValue(sunSpecPluginTimeoutParamTypeId).toUInt());
        connection->setNumberOfRetries(configValue(sunSpecPluginNumberOfRetriesParamTypeId).toUInt());
    }

    /* Reconnect on monitor reachable changed.
     *
     * Only the primary connection follows the monitor. The meter one is driven
     * by the primary's discovery instead, so that the two never handshake at the
     * same time towards the same inverter. */
    if (isPrimary) {
        NetworkDeviceMonitor *monitor = m_monitors.value(thing);
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcSunSpec()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (!thing->setupComplete())
                return;

            if (reachable && !thing->stateValue("connected").toBool()) {
                qCDebug(dcSunSpec()) << "The monitor is reachable. Set the host address to" << monitor->networkDeviceInfo().address() << "and start connecting...";
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->connectDevice();
            }
        });
    }

    connections.insert(thing->id(), connection);

    // Update all child things connected states for this connection
    connect(connection, &SunSpecConnection::connectedChanged, thing, [this, connection, thing, isPrimary] (bool connected) {
        if (connected) {
            qCDebug(dcSunSpec()) << connection << "connected";
        } else {
            qCWarning(dcSunSpec()) << connection << "disconnected";
        }

        /* The connected state of the thing follows the primary connection alone.
         * A Smart Meter that drops must not mark the inverter unreachable, nor
         * blank the data it is still delivering. */
        if (isPrimary)
            thing->setStateValue("connected", connected);

        if (connected) {
            if (thing->setupComplete()) {
                // We rediscovery all models on every connect to keep the child things in sync
                connection->startDiscovery();
            }
        }
        // Update connected state of the child things of this connection
        foreach (Thing *child, childThingsOn(thing, connection)) {
            if (connected) {
                child->setStateValue("connected", true);
            } else {
                markThingStatesDisconnected(child);
            }
        }
    });

    connect(connection, &SunSpecConnection::discoveryFinished, thing, [this, connection, thing, isPrimary] (bool success) {
        if (success) {
            qCDebug(dcSunSpec()) << "Discovery finished successfully of" << connection;
            processDiscoveryResult(thing, connection);
        } else {
            qCWarning(dcSunSpec()) << "Discovery finished with errors on" << connection;
        }

        /* Whether it succeeded or not, the primary connection is done talking:
         * this is the quietest moment to bring the Smart Meter one up. Two
         * simultaneous SunSpec discoveries would be the heaviest load this
         * plugin ever puts on an inverter, and the cost of avoiding it is a few
         * seconds of pairing. */
        if (isPrimary)
            startMeterConnection(thing);
    });

    /* m_discoveredConnections is keyed on the pointer, so it must not outlive
     * it. The entry is only ever compared, never dereferenced. */
    connect(connection, &QObject::destroyed, this, [this](QObject *object) {
        m_discoveredConnections.remove(static_cast<SunSpecConnection *>(object));
    });

    return connection;
}

QList<SunSpecConnection *> IntegrationPluginSunSpec::connectionsFor(const ThingId &parentId) const
{
    QList<SunSpecConnection *> connections;

    SunSpecConnection *primary = m_sunSpecConnections.value(parentId);
    if (primary)
        connections.append(primary);

    SunSpecConnection *meter = m_meterConnections.value(parentId);
    if (meter)
        connections.append(meter);

    return connections;
}

QList<SunSpecConnection *> IntegrationPluginSunSpec::allConnections() const
{
    QList<SunSpecConnection *> connections = m_sunSpecConnections.values();
    connections.append(m_meterConnections.values());
    return connections;
}

SunSpecConnection *IntegrationPluginSunSpec::createMeterConnection(Thing *thing)
{
    if (thing->thingClassId() != froniusConnectionThingClassId)
        return nullptr;

    const int inverterSlaveId = thing->paramValue(m_connectionSlaveIdParamTypeIds.value(thing->thingClassId())).toInt();
    const int meterSlaveId = thing->paramValue(froniusConnectionThingMeterSlaveIdParamTypeId).toInt();

    switch (FroniusRules::decideMeterConnection(meterSlaveId, inverterSlaveId)) {
    case FroniusRules::MeterConnectionDisabled:
        qCInfo(dcSunSpec()) << "No Smart Meter configured on" << thing->name()
                            << "- the meter slave ID is 0.";
        return nullptr;
    case FroniusRules::MeterConnectionInvalidSlaveId:
        qCWarning(dcSunSpec()) << "Not connecting a Smart Meter on" << thing->name()
                               << "- the meter slave ID" << meterSlaveId << "is out of range.";
        return nullptr;
    case FroniusRules::MeterConnectionCollidesWithInverter:
        qCWarning(dcSunSpec()) << "Not connecting a Smart Meter on" << thing->name()
                               << "- the meter slave ID" << meterSlaveId
                               << "is the one the inverter already answers on. A second connection there"
                               << "would rediscover the very same models and duplicate every child thing.";
        return nullptr;
    case FroniusRules::MeterConnectionCreate:
        break;
    }

    qCDebug(dcSunSpec()) << "Creating the Smart Meter connection of" << thing->name() << "on slave ID" << meterSlaveId;
    return createConnection(thing, meterSlaveId, false);
}

void IntegrationPluginSunSpec::startMeterConnection(Thing *thing)
{
    SunSpecConnection *meterConnection = m_meterConnections.value(thing->id());
    if (!meterConnection)
        return;

    if (meterConnection->connected()) {
        /* Already up. Rediscover anyway, so that a meter added or removed while
         * running is picked up, exactly like the primary connection does. */
        meterConnection->startDiscovery();
        return;
    }

    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    if (!monitor)
        return;

    qCDebug(dcSunSpec()) << "Primary discovery of" << thing->name() << "is done, connecting the Smart Meter on slave ID"
                         << meterConnection->slaveId();
    meterConnection->setHostAddress(monitor->networkDeviceInfo().address());
    meterConnection->connectDevice();
}

void IntegrationPluginSunSpec::destroyConnections(const ThingId &thingId)
{
    if (m_sunSpecConnections.contains(thingId))
        m_sunSpecConnections.take(thingId)->deleteLater();

    if (m_meterConnections.contains(thingId))
        m_meterConnections.take(thingId)->deleteLater();

    m_orphanSweepDeadlines.remove(thingId);
}

SunSpecConnection *IntegrationPluginSunSpec::connectionForChild(Thing *child) const
{
    if (m_sunSpecInverters.contains(child))
        return m_sunSpecInverters.value(child)->connection();

    if (m_sunSpecMeters.contains(child))
        return m_sunSpecMeters.value(child)->connection();

    if (m_sunSpecStorages.contains(child))
        return m_sunSpecStorages.value(child)->connection();

    if (m_sunSpecThings.contains(child))
        return m_sunSpecThings.value(child)->connection();

    return nullptr;
}

SunSpecModel *IntegrationPluginSunSpec::findModelFor(const ThingId &parentId, uint modelId, int modbusStartRegister) const
{
    foreach (SunSpecConnection *connection, connectionsFor(parentId)) {
        foreach (SunSpecModel *model, connection->models()) {
            if (model->modelId() == modelId && model->modbusStartRegister() == modbusStartRegister)
                return model;
        }
    }

    return nullptr;
}

QList<Thing *> IntegrationPluginSunSpec::childThingsOn(Thing *parent, SunSpecConnection *connection) const
{
    const bool isPrimary = (m_sunSpecConnections.value(parent->id()) == connection);

    QList<Thing *> children;
    foreach (Thing *child, myThings().filterByParentId(parent->id())) {
        SunSpecConnection *childConnection = connectionForChild(child);
        if (childConnection == connection || (!childConnection && isPrimary))
            children.append(child);
    }

    return children;
}

void IntegrationPluginSunSpec::setupSolarEdgeBattery(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    int modbusStartRegister = thing->paramValue(solarEdgeBatteryThingModbusAddressParamTypeId).toUInt();
    SunSpecConnection *connection = m_sunSpecConnections.value(thing->parentId());
    if (!connection) {
        qCWarning(dcSunSpec()) << "Could not find SunSpec parent connection for sunspec battery" << thing;
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    qCDebug(dcSunSpec()) << "Setting up SolarEdge battery...";
    SolarEdgeBattery *battery = new SolarEdgeBattery(thing, connection, modbusStartRegister, connection);

    m_sunSpecThings.insert(thing, battery);
    connect(battery, &SolarEdgeBattery::blockDataUpdated, this, &IntegrationPluginSunSpec::onSolarEdgeBatteryBlockUpdated);
    info->finish(Thing::ThingErrorNoError);

    // Start initializing battery data
    if (connection->connected())
        battery->init();
}

void IntegrationPluginSunSpec::searchSolarEdgeBatteries(SunSpecConnection *connection)
{
    if (!connection->connected()) {
        qCDebug(dcSunSpec()) << "Could not search for SolarEdge batteries, the connection does not seem to be available at the moment.";
        return;
    }

    qCDebug(dcSunSpec()) << "Searching for connected SolarEdge batteries...";
    ThingId parentThingId = m_sunSpecConnections.key(connection);
    if (parentThingId.isNull()) {
        qCWarning(dcSunSpec()) << "Could not search for SolarEdge batteries because of find parent ThingId connection for" << connection->hostAddress().toString();
        return;
    }

    // Batteries are not mapped to the sunspec layer, so we have to treat them as normal modbus registers.
    // Read the battery id to verify if the battery is connected.
    // Battery 1: start register 0xE100, device id register 0xE140
    // Battery 2: start register 0xE200, device id register 0xE240
    searchSolarEdgeBattery(connection, parentThingId, 0xE100);
    searchSolarEdgeBattery(connection, parentThingId, 0xE200);
}

void IntegrationPluginSunSpec::searchSolarEdgeBattery(SunSpecConnection *connection, const ThingId &parentThingId, quint16 startRegister)
{
    // Read the battery data, if init failed there is no battery connected,
    // otherwise there is a battery and we should check if we need to setup the battery thing

    // Create a temporary battery object without thing for init
    qCDebug(dcSunSpec()) << "Checking presence of SolarEdge battery on modbus register" << startRegister;
    SolarEdgeBattery *battery = new SolarEdgeBattery(nullptr, connection, startRegister, connection);
    connect(battery, &SolarEdgeBattery::initFinished, connection, [=](bool success) {
        // Delete this object since we used it only for set up
        battery->deleteLater();

        // If init failed, no battery connected
        if (!success) {
            qCDebug(dcSunSpec()) << "No SolarEdge battery connected on register" << startRegister << "- not creating thing.";
            return;
        }

        qCDebug(dcSunSpec()) << "Battery initialized successfully." << battery->batteryData().manufacturerName << battery->batteryData().model;
        // Check if we already created this battery
        if (!myThings().filterByParam(solarEdgeBatteryThingSerialNumberParamTypeId, battery->batteryData().serialNumber).isEmpty()) {
            qCDebug(dcSunSpec()) << "Battery already set up" << battery->batteryData().serialNumber;
        } else {
            // Create new battery device in the system
            ThingDescriptor descriptor(solarEdgeBatteryThingClassId, battery->batteryData().manufacturerName + " - " + battery->batteryData().model, QString(), parentThingId);
            ParamList params;
            params.append(Param(solarEdgeBatteryThingModbusAddressParamTypeId, startRegister));
            params.append(Param(solarEdgeBatteryThingManufacturerParamTypeId, battery->batteryData().manufacturerName));
            params.append(Param(solarEdgeBatteryThingDeviceModelParamTypeId, battery->batteryData().model));
            params.append(Param(solarEdgeBatteryThingSerialNumberParamTypeId, battery->batteryData().serialNumber));
            descriptor.setParams(params);
            emit autoThingsAppeared({descriptor});
        }
    });

    // Try to initialize battery data
    battery->init();
}

double IntegrationPluginSunSpec::calculateSolarEdgePvProduction(Thing *thing, double acPower, double dcPower)
{
    // Note: for solar edge, we need to calculate pv power from battery and AC/DC current depending
    // on loading or discharging of the battery

    double pvPower = acPower;
    Thing *parentThing = myThings().findById(thing->parentId());
    if (parentThing && parentThing->thingClassId() == solarEdgeConnectionThingClassId) {
        SolarEdgeBattery *battery = nullptr;
        // This is a solar edge, let's see if we have a battery for this connection
        foreach (Thing *sunspecThing, m_sunSpecThings.keys()) {
            if (sunspecThing->thingClassId() == solarEdgeBatteryThingClassId && thing->parentId() == parentThing->id()) {
                battery = qobject_cast<SolarEdgeBattery *>(m_sunSpecThings.value(sunspecThing));
                break;
            }
        }

        // Check if we have a meter...
        Thing *meterThing = nullptr;
        foreach (Thing *sunspecThing, m_sunSpecMeters.keys()) {
            if (thing->parentId() == parentThing->id()) {
                meterThing = sunspecThing;
                break;
            }
        }

        // This is a solar edge, let's see if we have a battery for this connection
        if (battery) {
            double meterCurrentPower = meterThing ? meterThing->stateValue("currentPower").toDouble() : 0;
            qCDebug(dcSunSpec()) << "SolarEdge: found battery for inverter: calculate actual PV power from battery DC power and inverter DC power...";
            qCDebug(dcSunSpec()) << "--> SolarEdge: -------------------------------------------------------";
            qCDebug(dcSunSpec()) << "--> SolarEdge: meter power:" << meterCurrentPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: inverter AC power:" << acPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: inverter DC power:" << dcPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: battery DC power:" << battery->batteryData().instantaneousPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: battery DC state:" << battery->batteryData().batteryStatus;

            switch (battery->batteryData().batteryStatus) {
            case SolarEdgeBattery::BatteryStatus::Charge: {
                // Actual PV = inverter AC power - battery power
                pvPower = acPower - battery->batteryData().instantaneousPower;
                qCDebug(dcSunSpec()) << "--> SolarEdge: calculate actual PV power: inverter AC power - battery power:" << acPower << "-" << battery->batteryData().instantaneousPower << "=" << pvPower;
                break;
            }
            case SolarEdgeBattery::BatteryStatus::Discharge: {
                // Actual PV = inverter DC power + battery power
                pvPower = dcPower - battery->batteryData().instantaneousPower;
                qCDebug(dcSunSpec()) << "--> SolarEdge: calculate actual PV power: inverter DC power - battery power:" << dcPower << "-" << battery->batteryData().instantaneousPower << "=" << pvPower;
                if (pvPower > 0) {
                    qCDebug(dcSunSpec()) << "--> SolarEdge: actual PV power: 0W | loss:" << pvPower << "W";
                    pvPower = 0;
                }
                break;
            }
            default:
                // Idle: No change required, AC power is actual PV energy
                pvPower = acPower;
                break;
            }

            // Calculate self consumption
            double selfConsumption = pvPower - meterCurrentPower + battery->batteryData().instantaneousPower;
            qCDebug(dcSunSpec()) << "--> SolarEdge: self consumption" << selfConsumption << "W";
        }
    }

    return pvPower;
}

SunSpecDuplicates::ModelFamily IntegrationPluginSunSpec::modelFamilyOf(Thing *thing) const
{
    if (m_modelIdParamTypeIds.contains(thing->thingClassId()))
        return SunSpecDuplicates::modelFamily(thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toUInt());

    // Not created from a SunSpec model, but a battery all the same.
    if (thing->thingClassId() == solarEdgeBatteryThingClassId)
        return SunSpecDuplicates::ModelFamilyStorage;

    return SunSpecDuplicates::ModelFamilyUnknown;
}

void IntegrationPluginSunSpec::autocreateSunSpecModelThing(const ThingClassId &thingClassId, const QString &thingName, Thing *parent, SunSpecModel *model)
{
    /* On some systems we get duplicated things having the same serial number but are on different registers.
     * Those devices normally dissapear after some time again. For now, we need to prevent to even create those
     * devices by filtering out those who already have been added using the serialnumber, not only the combination
     * of model number and start address.
     *
     * The serial number belongs to the common block, so every model behind it carries the same one: a storage
     * model 124 behind the common block of its inverter is not a duplicate of that inverter. Only a thing of the
     * same kind of model counts, see SunSpecDuplicates. */

    QString serialNumber = model->commonModelInfo().serialNumber;
    if (!serialNumber.isEmpty()) {
        const SunSpecDuplicates::ModelFamily family = SunSpecDuplicates::modelFamily(model->modelId());
        foreach (Thing *thing, myThings()) {
            if (!m_serialNumberParamTypeIds.contains(thing->thingClassId()))
                continue;

            const SunSpecDuplicates::KnownThing known = { thing->paramValue(m_serialNumberParamTypeIds.value(thing->thingClassId())).toString(),
                                                          modelFamilyOf(thing) };
            if (SunSpecDuplicates::isDuplicate(serialNumber, family, { known })) {
                qCWarning(dcSunSpec()) << "Trying to add a new thing for a SunSpec model, but we already have a thing of the same kind with this serial number. Not adding it to the system...";
                qCWarning(dcSunSpec()) << "--> Already added:" << thing << thing->params();
                qCWarning(dcSunSpec()) << "--> Attempt to add:" << thingName << model
                                       << model->commonModelInfo().modelName
                                       << model->commonModelInfo().manufacturerName
                                       << model->commonModelInfo().serialNumber
                                       << model->commonModelInfo().versionString;

                return;
            }
        }
    }

    ThingDescriptor descriptor(thingClassId);
    descriptor.setParentId(parent->id());
    QString finalThingName;
    if (model->commonModelInfo().manufacturerName.isEmpty()) {
        finalThingName = thingName;
    } else {
        finalThingName = model->commonModelInfo().manufacturerName + " " + thingName;
    }

    descriptor.setTitle(finalThingName);

    ParamList params;
    params.append(Param(m_modelIdParamTypeIds.value(descriptor.thingClassId()), model->modelId()));
    params.append(Param(m_modbusAddressParamTypeIds.value(descriptor.thingClassId()), model->modbusStartRegister()));
    params.append(Param(m_manufacturerParamTypeIds.value(descriptor.thingClassId()), model->commonModelInfo().manufacturerName));
    params.append(Param(m_deviceModelParamTypeIds.value(descriptor.thingClassId()), model->commonModelInfo().modelName));
    params.append(Param(m_serialNumberParamTypeIds.value(descriptor.thingClassId()), model->commonModelInfo().serialNumber));

    /* A storage created under a Fronius connection arms the revert timer by
     * default, where the generic vendor leaves it disabled. A setpoint left
     * behind by a stopped HEMS would otherwise hold the customer's battery
     * indefinitely.
     *
     * Asked of the parent thing the discovery came from, not through
     * isFroniusChild(): on the very first install the discovery finishes
     * inside setupThing(), before the parent is registered in myThings(), and
     * the lookup would leave the timer disabled on exactly the battery
     * installed together with its inverter. */
    if (thingClassId == sunspecStorageThingClassId && parent->thingClassId() == froniusConnectionThingClassId) {
        params.append(Param(sunspecStorageThingRevertTimeParamTypeId, FroniusRules::DefaultRevertTimeSeconds));
    }

    descriptor.setParams(params);
    emit autoThingsAppeared({descriptor});
}

QString IntegrationPluginSunSpec::getInverterStateString(quint16 state)
{
    // Note: this works because the models have all the same value
    QString stateString;
    switch (state) {
    case SunSpecInverterSinglePhaseModel::StOff:
        stateString = "Off";
        break;
    case SunSpecInverterSinglePhaseModel::StSleeping:
        stateString = "Sleeping";
        break;
    case SunSpecInverterSinglePhaseModel::StStarting:
        stateString = "Starting";
        break;
    case SunSpecInverterSinglePhaseModel::StMppt:
        stateString = "MPPT";
        break;
    case SunSpecInverterSinglePhaseModel::StThrottled:
        stateString = "Throttled";
        break;
    case SunSpecInverterSinglePhaseModel::StShuttingDown:
        stateString = "Shutting down";
        break;
    case SunSpecInverterSinglePhaseModel::StFault:
        stateString = "Fault";
        break;
    case SunSpecInverterSinglePhaseModel::StStandby:
        stateString = "Standby";
        break;
    }
    return stateString;
}

QString IntegrationPluginSunSpec::getInverterErrorString(quint32 flag)
{
    // Note: this works because the models have all the same value
    QStringList errorStrings;
    SunSpecInverterSinglePhaseModel::Evt1Flags event1Flag = static_cast<SunSpecInverterSinglePhaseModel::Evt1Flags>(flag);
    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1GroundFault)) {
        errorStrings.append(QT_TR_NOOP("Ground fault"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1DcOverVolt)) {
        errorStrings.append(QT_TR_NOOP("DC over voltage"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1AcDisconnect)) {
        errorStrings.append(QT_TR_NOOP("AC disconnect"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1DcDisconnect)) {
        errorStrings.append(QT_TR_NOOP("DC disconnect"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1GridDisconnect)) {
        errorStrings.append(QT_TR_NOOP("Grid disconnect"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1CabinetOpen)) {
        errorStrings.append(QT_TR_NOOP("Cabinet open"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1ManualShutdown)) {
        errorStrings.append(QT_TR_NOOP("Manual shutdown"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1OverTemp)) {
        errorStrings.append(QT_TR_NOOP("Over temperature"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1OverFrequency)) {
        errorStrings.append(QT_TR_NOOP("Over frequency"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1UnderFrequency)) {
        errorStrings.append(QT_TR_NOOP("Under frequency"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1AcOverVolt)) {
        errorStrings.append(QT_TR_NOOP("AC over voltage"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1AcUnderVolt)) {
        errorStrings.append(QT_TR_NOOP("AC under voltage"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1BlownStringFuse)) {
        errorStrings.append(QT_TR_NOOP("Blown string fuse"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1UnderTemp)) {
        errorStrings.append(QT_TR_NOOP("Under temperature"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1MemoryLoss)) {
        errorStrings.append(QT_TR_NOOP("Memory loss"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1HwTestFailure)) {
        errorStrings.append(QT_TR_NOOP("Hardware test failure"));
    }

    if (errorStrings.isEmpty()) {
        return QT_TR_NOOP("No error");
    } else {
        return errorStrings.join(", ");
    }
}

double IntegrationPluginSunSpec::fixValueSign(double targetValue, double powerValue)
{
    // Some sunspec devices (i.e. SolarEdge return an absolute value on the phase current.
    // This method makes sure the phase current has the same sign as the phase power value.
    bool sameSign = ((targetValue < 0) == (powerValue < 0));
    return (sameSign ? targetValue : -targetValue);
}

bool IntegrationPluginSunSpec::hasManufacturer(const QStringList &manufacturers, const QString &manufacturer)
{
    foreach (const QString &name, manufacturers) {
        if (name.toLower().contains(manufacturer.toLower())) {
            return true;
        }
    }

    return false;
}

void IntegrationPluginSunSpec::markThingStatesDisconnected(Thing *thing)
{
    qCDebug(dcSunSpec()) << thing << "is now disconnected. Setting energy live data to 0.";
    if (thing->thingClassId() == sunspecSinglePhaseInverterThingClassId) {
        thing->setStateValue(sunspecSinglePhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterPhaseVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecSplitPhaseInverterThingClassId) {
        thing->setStateValue(sunspecSplitPhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseANVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseACurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecThreePhaseInverterThingClassId) {
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << inverter;
        thing->setStateValue(sunspecThreePhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseANVoltageStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseBNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseCNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseACurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseBCurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseCCurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecSinglePhaseMeterThingClassId) {
        thing->setStateValue(sunspecSinglePhaseMeterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseMeterVoltagePhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseMeterFrequencyStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecSplitPhaseMeterThingClassId) {
        thing->setStateValue(sunspecSplitPhaseMeterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterLnACVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseBStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterFrequencyStateTypeId,0 );
    } else if (thing->thingClassId() == sunspecThreePhaseMeterThingClassId) {
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, false);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecStorageThingClassId) {
        thing->setStateValue(sunspecStorageConnectedStateTypeId, false);
        /* Zeroed, like every other power state of this plugin: nymea's power
         * balance keeps summing a disconnected energystorage, and a stale
         * value would be asserted as a measurement long after the inverter
         * stopped answering. `capacity` is deliberately left alone - it is a
         * nameplate figure, and losing the link does not shrink the battery. */
        thing->setStateValue(sunspecStorageCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecStorageGridChargingStateTypeId, false);
        thing->setStateValue(sunspecStorageChargingRateStateTypeId, 0);
        thing->setStateValue(sunspecStorageDischargingRateStateTypeId, 0);
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Off");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "idle");
    } else if (thing->thingClassId() == solarEdgeBatteryThingClassId) {
        thing->setStateValue(solarEdgeBatteryConnectedStateTypeId, false);
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Idle");
        thing->setStateValue(solarEdgeBatteryChargingStateStateTypeId, "idle");
        thing->setStateValue(solarEdgeBatteryInstantaneousVoltageStateTypeId, 0);
        thing->setStateValue(solarEdgeBatteryInstantaneousCurrentStateTypeId, 0);
        thing->setStateValue(solarEdgeBatteryCurrentPowerStateTypeId, 0);
    }

}

void IntegrationPluginSunSpec::onRefreshTimer()
{
    // Update meters
    foreach (SunSpecModel *model, m_sunSpecMeters.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update storage
    foreach (SunSpecModel *model, m_sunSpecStorages.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    /* Companion models of the storage things. Deduplicated: two storage things
     * on the same inverter share one model 160, and reading it twice per cycle
     * would double the Modbus traffic for the same 90 registers.
     *
     * Whole model, one readBlockData() each, never a partial read: the power of
     * the battery blocks is only meaningful next to the scale factor of the
     * same transaction. See the read order note above
     * storageModuleReadings(). */
    QSet<SunSpecModel *> storageCompanions;
    foreach (SunSpecMpptModel *model, m_storageMpptModels.values())
        storageCompanions.insert(model);
    foreach (SunSpecNameplateModel *model, m_storageNameplateModels.values())
        storageCompanions.insert(model);

    foreach (SunSpecModel *model, storageCompanions) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update all other sunspec thing blocks
    foreach (SunSpecThing *sunSpecThing, m_sunSpecThings) {
        if (sunSpecThing->connection()->connected()) {
            sunSpecThing->readBlockData();
        }
    }

    // Update inverters
    foreach (SunSpecModel *model, m_sunSpecInverters.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }
}

void IntegrationPluginSunSpec::onPluginConfigurationChanged(const ParamTypeId &paramTypeId, const QVariant &value)
{
    // Check refresh schedule
    if (paramTypeId == sunSpecPluginUpdateIntervalParamTypeId) {
        qCDebug(dcSunSpec()) << "Update interval has changed" << value.toInt();
        if (m_refreshTimer) {
            int refreshTime = effectiveUpdateInterval();
            m_refreshTimer->stop();
            hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
            m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(refreshTime);
            connect(m_refreshTimer, &PluginTimer::timeout, this, &IntegrationPluginSunSpec::onRefreshTimer);
            m_refreshTimer->start();
        }
    } else if (paramTypeId == sunSpecPluginNumberOfRetriesParamTypeId) {
        qCDebug(dcSunSpec()) << "Updating number of retries" << value.toUInt();
        foreach (SunSpecConnection *connection, allConnections()) {
            connection->setNumberOfRetries(value.toUInt());
        }
    } else if (paramTypeId == sunSpecPluginTimeoutParamTypeId) {
        qCDebug(dcSunSpec()) << "Updating timeout" << value.toUInt() << "[ms]";
        foreach (SunSpecConnection *connection, allConnections()) {
            connection->setTimeout(value.toUInt());
        }
    } else {
        qCWarning(dcSunSpec()) << "Unknown plugin configuration" << paramTypeId << "Value" << value;
    }
}

void IntegrationPluginSunSpec::onInverterBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecInverters.key(model);
    if (!thing) return;

    // Get parent thing
    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing)
        return;

    switch (model->modelId()) {
    case SunSpecModelFactory::ModelIdInverterSinglePhase: {
        SunSpecInverterSinglePhaseModel *inverter = qobject_cast<SunSpecInverterSinglePhaseModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << inverter;
        thing->setStateValue(sunspecSinglePhaseInverterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSinglePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);

        // Note: solar edge needs some calculations for the current pv power
        double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerStateTypeId, currentPower);
        evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseInverterTotalCurrentStateTypeId, inverter->amps());
        thing->setStateValue(sunspecSinglePhaseInverterFrequencyStateTypeId, inverter->hz());
        thing->setStateValue(sunspecSinglePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
        thing->setStateValue(sunspecSinglePhaseInverterPhaseVoltageStateTypeId, inverter->phaseVoltageAn());
        thing->setStateValue(sunspecSinglePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
        thing->setStateValue(sunspecSinglePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
        thing->setStateValue(sunspecSinglePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
        thing->setStateValue(sunspecSinglePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        break;
    }
    case SunSpecModelFactory::ModelIdInverterSinglePhaseFloat: {
        SunSpecInverterSinglePhaseFloatModel *inverter = qobject_cast<SunSpecInverterSinglePhaseFloatModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << inverter;
        thing->setStateValue(sunspecSinglePhaseInverterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSinglePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);

        // Note: solar edge needs some calculations for the current pv power
        double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerStateTypeId, currentPower);
        evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseInverterTotalCurrentStateTypeId, inverter->amps());
        thing->setStateValue(sunspecSinglePhaseInverterFrequencyStateTypeId, inverter->hz());
        thing->setStateValue(sunspecSinglePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
        thing->setStateValue(sunspecSinglePhaseInverterPhaseVoltageStateTypeId, inverter->phaseVoltageAn());
        thing->setStateValue(sunspecSinglePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
        thing->setStateValue(sunspecSinglePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
        thing->setStateValue(sunspecSinglePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
        thing->setStateValue(sunspecSinglePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        break;
    }
    case SunSpecModelFactory::ModelIdInverterSplitPhase: {
        SunSpecInverterSplitPhaseModel *inverter = qobject_cast<SunSpecInverterSplitPhaseModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << inverter;
        thing->setStateValue(sunspecSplitPhaseInverterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSplitPhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);

        double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerStateTypeId, currentPower);
        evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseInverterTotalCurrentStateTypeId, inverter->amps());
        thing->setStateValue(sunspecSplitPhaseInverterFrequencyStateTypeId, inverter->hz());
        thing->setStateValue(sunspecSplitPhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
        thing->setStateValue(sunspecSplitPhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
        thing->setStateValue(sunspecSplitPhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
        thing->setStateValue(sunspecSplitPhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
        thing->setStateValue(sunspecSplitPhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        break;
    }
    case SunSpecModelFactory::ModelIdInverterSplitPhaseFloat: {
        SunSpecInverterSplitPhaseFloatModel *inverter = qobject_cast<SunSpecInverterSplitPhaseFloatModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << inverter;
        thing->setStateValue(sunspecSplitPhaseInverterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSplitPhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);

        double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerStateTypeId, currentPower);
        evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseInverterTotalCurrentStateTypeId, inverter->amps());
        thing->setStateValue(sunspecSplitPhaseInverterFrequencyStateTypeId, inverter->hz());
        thing->setStateValue(sunspecSplitPhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
        thing->setStateValue(sunspecSplitPhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
        thing->setStateValue(sunspecSplitPhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
        thing->setStateValue(sunspecSplitPhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
        thing->setStateValue(sunspecSplitPhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        break;
    }
    case SunSpecModelFactory::ModelIdInverterThreePhase: {
        SunSpecInverterThreePhaseModel *inverter = qobject_cast<SunSpecInverterThreePhaseModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << inverter;
        thing->setStateValue(sunspecThreePhaseInverterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);

        double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerStateTypeId, currentPower);
        evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
        thing->setStateValue(sunspecThreePhaseInverterTotalCurrentStateTypeId, inverter->amps());
        thing->setStateValue(sunspecThreePhaseInverterFrequencyStateTypeId, inverter->hz());
        thing->setStateValue(sunspecThreePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
        thing->setStateValue(sunspecThreePhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseInverterPhaseCNVoltageStateTypeId, inverter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
        thing->setStateValue(sunspecThreePhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
        thing->setStateValue(sunspecThreePhaseInverterPhaseCCurrentStateTypeId, inverter->ampsPhaseC());
        thing->setStateValue(sunspecThreePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
        thing->setStateValue(sunspecThreePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
        thing->setStateValue(sunspecThreePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
        thing->setStateValue(sunspecThreePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        break;
    }
    case SunSpecModelFactory::ModelIdInverterThreePhaseFloat: {
        SunSpecInverterThreePhaseFloatModel *inverter = qobject_cast<SunSpecInverterThreePhaseFloatModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << inverter;
        thing->setStateValue(sunspecThreePhaseInverterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);

        double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerStateTypeId, currentPower);
        evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
        thing->setStateValue(sunspecThreePhaseInverterTotalCurrentStateTypeId, inverter->amps());
        thing->setStateValue(sunspecThreePhaseInverterFrequencyStateTypeId, inverter->hz());
        thing->setStateValue(sunspecThreePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
        thing->setStateValue(sunspecThreePhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseInverterPhaseCNVoltageStateTypeId, inverter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
        thing->setStateValue(sunspecThreePhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
        thing->setStateValue(sunspecThreePhaseInverterPhaseCCurrentStateTypeId, inverter->ampsPhaseC());
        thing->setStateValue(sunspecThreePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
        thing->setStateValue(sunspecThreePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
        thing->setStateValue(sunspecThreePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
        thing->setStateValue(sunspecThreePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        break;
    }
    default:
        qCWarning(dcSunSpec()) << "Received block data from unhandled model" << model;
        break;
    }
}

void IntegrationPluginSunSpec::onMeterBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecMeters.key(model);
    if (!thing) return;

    // Get parent thing
    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing)
        return;

    /* Sign applied to every power reading of this block. The generic vendors
     * keep the historical negation; a meter sitting under a Fronius connection
     * already reports in the nymea convention. See FroniusRules. */
    const double powerSign = FroniusRules::meterPowerSign(
        parentThing->thingClassId() == froniusConnectionThingClassId);

    switch (model->modelId()) {
    case SunSpecModelFactory::ModelIdMeterSinglePhase: {
        SunSpecMeterSinglePhaseModel *meter = qobject_cast<SunSpecMeterSinglePhaseModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecSinglePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPhaseAStateTypeId, powerSign * meter->ampsPhaseA());
        thing->setStateValue(sunspecSinglePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSinglePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSinglePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterSinglePhaseFloat: {
        SunSpecMeterSinglePhaseFloatModel *meter = qobject_cast<SunSpecMeterSinglePhaseFloatModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecSinglePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), powerSign * meter->watts()));
        thing->setStateValue(sunspecSinglePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSinglePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSinglePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseAbn: {
        SunSpecMeterSplitSinglePhaseAbnModel *meter = qobject_cast<SunSpecMeterSplitSinglePhaseAbnModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecSplitPhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecSplitPhaseMeterTotalCurrentStateTypeId, fixValueSign(meter->amps(), powerSign * meter->watts()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseAStateTypeId, powerSign * meter->wattsPhaseA());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseBStateTypeId, powerSign * meter->wattsPhaseB());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), powerSign * meter->wattsPhaseA()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), powerSign * meter->wattsPhaseB()));
        thing->setStateValue(sunspecSplitPhaseMeterLnACVoltageStateTypeId, meter->voltageLn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecSplitPhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSplitPhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseFloat: {
        SunSpecMeterSplitSinglePhaseFloatModel *meter = qobject_cast<SunSpecMeterSplitSinglePhaseFloatModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecSplitPhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecSplitPhaseMeterTotalCurrentStateTypeId, fixValueSign(meter->amps(), powerSign * meter->watts()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseAStateTypeId, powerSign * meter->wattsPhaseA());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseBStateTypeId, powerSign * meter->wattsPhaseB());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), powerSign * meter->wattsPhaseA()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), powerSign * meter->wattsPhaseB()));
        thing->setStateValue(sunspecSplitPhaseMeterLnACVoltageStateTypeId, meter->voltageLn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecSplitPhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSplitPhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterThreePhase: {
        SunSpecMeterThreePhaseModel *meter = qobject_cast<SunSpecMeterThreePhaseModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, powerSign * meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, powerSign * meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, powerSign * meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), powerSign * meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), powerSign * meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), powerSign * meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdDeltaConnectThreePhaseAbcMeter: {
        SunSpecDeltaConnectThreePhaseAbcMeterModel *meter = qobject_cast<SunSpecDeltaConnectThreePhaseAbcMeterModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, powerSign * meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, powerSign * meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, powerSign * meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), powerSign * meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), powerSign * meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), powerSign * meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterThreePhaseWyeConnect: {
        SunSpecMeterThreePhaseWyeConnectModel *meter = qobject_cast<SunSpecMeterThreePhaseWyeConnectModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, powerSign * meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, powerSign * meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, powerSign * meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), powerSign * meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), powerSign * meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), powerSign * meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterThreePhaseDeltaConnect: {
        SunSpecMeterThreePhaseDeltaConnectModel *meter = qobject_cast<SunSpecMeterThreePhaseDeltaConnectModel *>(model);
        qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << meter;
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, powerSign * meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, powerSign * meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, powerSign * meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, powerSign * meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), powerSign * meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), powerSign * meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), powerSign * meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    default:
        qCWarning(dcSunSpec()) << "Received block data from unhandled model" << model;
        break;
    }
}

void IntegrationPluginSunSpec::onStorageBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecStorages.key(model);
    if (!thing) return;

    SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(model);
    qCDebug(dcSunSpec()) << thing->name() << "block data updated";// << storage;

    thing->setStateValue(sunspecStorageConnectedStateTypeId, true);
    thing->setStateValue(sunspecStorageVersionStateTypeId, model->commonModelInfo().versionString);

    const SunSpecStorageLogic::StorageStates states = SunSpecStorageLogic::readStates(storage);

    thing->setStateValue(sunspecStorageBatteryCriticalStateTypeId, states.batteryCritical);
    thing->setStateValue(sunspecStorageBatteryLevelStateTypeId, states.batteryLevel);
    thing->setStateValue(sunspecStorageGridChargingStateTypeId, states.gridCharging);
    thing->setStateValue(sunspecStorageEnableChargingStateTypeId, states.enableCharging);
    thing->setStateValue(sunspecStorageEnableDischargingStateTypeId, states.enableDischarging);
    thing->setStateValue(sunspecStorageChargingRateStateTypeId, states.chargingRate);
    thing->setStateValue(sunspecStorageDischargingRateStateTypeId, states.dischargingRate);
    thing->setStateValue(sunspecStorageChargeGradientStateTypeId, states.chargeGradient);
    thing->setStateValue(sunspecStorageDischargeGradientStateTypeId, states.dischargeGradient);

    switch (storage->chaSt()) {
    case SunSpecStorageModel::ChastOff:
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Off");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "idle");
        break;
    case SunSpecStorageModel::ChastEmpty:
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Empty");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "idle");
        break;
    case SunSpecStorageModel::ChastDischarging:
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Discharging");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "discharging");
        break;
    case SunSpecStorageModel::ChastCharging:
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Charging");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "charging");
        break;
    case SunSpecStorageModel::ChastFull:
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Full");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "idle");
        break;
    case SunSpecStorageModel::ChastHolding:
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Holding");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "idle");
        break;
    case SunSpecStorageModel::ChastTesting:
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Testing");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "idle");
        break;
    }
}

/* Model 124 describes what the battery IS - its state of charge, its status,
 * its setpoints - and nothing about what it is DOING in watts. There is no
 * power point anywhere in that model. The instantaneous power of a Fronius
 * storage lives in model 160, split over two blocks named `StCha` and
 * `StDisCha`, and the rated capacity lives in WHRtg of model 120.
 *
 * Hence this binding: the storage thing stays keyed on its model 124, and
 * borrows the two other models of the same connection to fill the two states
 * the energystorage interface requires. */
void IntegrationPluginSunSpec::bindStorageCompanionModels(Thing *thing, SunSpecModel *storageModel)
{
    if (!thing || !storageModel || !storageModel->connection())
        return;

    /* The same connection as the model 124, never a search across the parent:
     * a Fronius thing carries two connections, and the model 160 of the
     * inverter must not be paired with a storage read from the meter unit. */
    SunSpecConnection *connection = storageModel->connection();

    foreach (SunSpecModel *model, connection->models()) {
        if (model->modelId() == SunSpecModelFactory::ModelIdMppt) {
            SunSpecMpptModel *mppt = qobject_cast<SunSpecMpptModel *>(model);
            if (mppt && m_storageMpptModels.value(thing) != mppt) {
                m_storageMpptModels.insert(thing, mppt);
                connect(mppt, &SunSpecModel::blockUpdated, this,
                        &IntegrationPluginSunSpec::onStorageMpptBlockUpdated, Qt::UniqueConnection);
                qCDebug(dcSunSpec()) << "Storage" << thing->name() << "takes its power from" << mppt;
            }
        } else if (model->modelId() == SunSpecModelFactory::ModelIdNameplate) {
            SunSpecNameplateModel *nameplate = qobject_cast<SunSpecNameplateModel *>(model);
            if (nameplate && m_storageNameplateModels.value(thing) != nameplate) {
                m_storageNameplateModels.insert(thing, nameplate);
                connect(nameplate, &SunSpecModel::blockUpdated, this,
                        &IntegrationPluginSunSpec::onStorageNameplateBlockUpdated, Qt::UniqueConnection);
                qCDebug(dcSunSpec()) << "Storage" << thing->name() << "takes its capacity from" << nameplate;
            }
        }
    }

    if (!m_storageMpptModels.contains(thing)) {
        /* Said once, and said loudly. This thing now claims the energystorage
         * interface, so nymea's power balance and the storage reserve of the
         * arbiter will read its currentPower - and it will never leave zero.
         * A filter that finds a battery reporting a flat zero is worse than a
         * filter that finds none: the first reads as "neither charging nor
         * discharging", the second as "no battery". */
        if (!m_storagesWarnedWithoutPowerSource.contains(thing->id())) {
            m_storagesWarnedWithoutPowerSource.insert(thing->id());
            qCWarning(dcSunSpec()) << "Storage" << thing->name() << "has no model 160 on its connection:"
                                   << "currentPower has no register to read and will stay at 0."
                                   << "This storage is visible to the energy balance but not measurable.";
        }
        return;
    }

    /* Publish straight away, so a thing bound after a discovery does not wait a
     * whole refresh cycle. Both updaters return quietly when their model has
     * nothing readable yet - a single guard, inside them, rather than a second
     * one here that would have to be kept in step. */
    updateStorageCurrentPower(thing);
    updateStorageCapacity(thing);
}

/* READ ORDER - do not "optimise" the reading of model 160.
 *
 * `dcPower()` returns a value ALREADY multiplied by `DCW_SF`. This scale factor
 * is not a constant of the device: Fronius auto-scales it DURING
 * OPERATION, without hysteresis. Measured on a test installation, same
 * inverter, two readings a few seconds apart:
 *
 *     battery idle, low PV            ->  DCW_SF = -2
 *     battery charging 2000 W         ->  DCW_SF = -1
 *
 * A raw `DCW` of 20000 is therefore 200.0 W with the first factor and 2000.0 W
 * with the second. Reading the power with a factor that is not the one of the
 * SAME transaction means being off by an order of magnitude - same kind of
 * damage as the word order of float32 values (D-8), but far more discreet: the
 * value stays plausible, it is just ten times too small or too large.
 *
 * What protects us today:
 *
 *  1. `SunSpecModel::readBlockData()` reads the 90 registers of model 160 in ONE
 *     Modbus transaction (the split limit is at 125 registers), so the
 *     scale factors and the powers come from the same snapshot;
 *  2. `SunSpecModel::handleNewBlockData()` calls `processBlockData()` of the
 *     FIXED block - which re-reads `DCA_SF`/`DCV_SF`/`DCW_SF`/`DCWH_SF` - BEFORE
 *     processing the repeating blocks, which read that factor from their parent.
 *
 * These two properties live in libnymea-sunspec, not here, and nothing in the
 * compiler enforces them. Breaking them upstream - splitting the read, caching
 * the factors at discovery, processing the repeating blocks first,
 * or reading a repeating block on its own - would break this function WITHOUT ANY
 * OUTSIDE SIGN: no Modbus error, no denormal, a device that reports itself
 * `connected`. Any rework of the model 160 read must be replayed in an
 * on-site test against a known battery power, not just compiled.
 *
 * INV-5 in sunspec/FRONIUS.md, and D-9 in DIVERGENCES.md. */
QList<FroniusRules::ModuleReading> IntegrationPluginSunSpec::storageModuleReadings(Thing *thing) const
{
    QList<FroniusRules::ModuleReading> readings;

    SunSpecMpptModel *mppt = m_storageMpptModels.value(thing);
    if (!mppt)
        return readings;

    /* DCWH_SF has the same read order guarantee as DCW_SF above: fixed block
     * first, in the same transaction as the cumulative energies it scales. */
    const bool energyScaleFactorImplemented = mppt->dataPoints().value("DCWH_SF").isValid();

    foreach (SunSpecModelRepeatingBlock *block, mppt->repeatingBlocks()) {
        SunSpecMpptModelRepeatingBlock *module = qobject_cast<SunSpecMpptModelRepeatingBlock *>(block);
        if (!module)
            continue;

        /* Keyed on the ID string, never on the block index: on a three or four
         * MPPT inverter the battery blocks are not in position 3 and 4. */
        readings.append({module->inputIdSting(), static_cast<double>(module->dcPower()),
                         FroniusRules::moduleLifetimeEnergyWh(module->lifetimeEnergy(), mppt->energyScaleFactor(),
                                                              energyScaleFactorImplemented)});
    }

    return readings;
}

void IntegrationPluginSunSpec::updateStorageCurrentPower(Thing *thing)
{
    const QList<FroniusRules::ModuleReading> readings = storageModuleReadings(thing);

    /* Not read yet, not "no battery". On the first blockUpdated the repeating
     * blocks have just been created and carry no data, so nothing classifies.
     * Warning here would put a false alarm in the journal of every healthy
     * machine at every start - on the very line operations is told to trust to
     * tell a broken storage from an unmeasurable one. Silence, and wait for the
     * next cycle: readBlockData() runs on every refresh anyway. */
    if (!FroniusRules::blocksAreIdentified(readings))
        return;

    if (!FroniusRules::hasBatteryBlocks(readings)) {
        /* An inverter with no battery publishes N=2 and no StCha/StDisCha at
         * all. Summing an empty set would answer a confident 0 W. */
        if (!m_storagesWarnedWithoutPowerSource.contains(thing->id())) {
            m_storagesWarnedWithoutPowerSource.insert(thing->id());
            qCWarning(dcSunSpec()) << "Storage" << thing->name() << "has a model 160 but no StCha/StDisCha block:"
                                   << "currentPower has nothing to read and will stay at 0.";
        }
        return;
    }

    m_storagesWarnedWithoutPowerSource.remove(thing->id());

    /* Charge minus discharge. The manufacturer counts the other way round and
     * so does FroniusRules::batteryPower(); the interface this state belongs to
     * counts a charging battery as consuming, hence positive. */
    const double currentPower = FroniusRules::storageCurrentPower(readings);
    thing->setStateValue(sunspecStorageCurrentPowerStateTypeId, currentPower);

    /* Charged energy is what the battery consumed, discharged energy what it
     * produced - the same convention as currentPower. */
    updateStorageEnergy(thing, sunspecStorageTotalEnergyConsumedStateTypeId, FroniusRules::storageChargedEnergyKwh(readings));
    updateStorageEnergy(thing, sunspecStorageTotalEnergyProducedStateTypeId, FroniusRules::storageDischargedEnergyKwh(readings));
}

/* Published as the device counts it, decreases included. A cumulative counter
 * that goes down has been reset on the device side, and nymea's energy manager
 * already handles that: it re-synchronises on the lower value and counts no
 * energy for that cycle, exactly as it does for a meter. Holding the old value
 * here instead would hide the reset and then swallow every kWh until the
 * device climbed back above it. */
void IntegrationPluginSunSpec::updateStorageEnergy(Thing *thing, const StateTypeId &stateTypeId, double energyKwh)
{
    if (energyKwh < 0) {
        /* Nothing publishable. The state keeps its value: the default zero, or
         * the last published total, both of which nymea reads as "no energy
         * moved", never as negative energy. */
        return;
    }

    const double previous = thing->stateValue(stateTypeId).toDouble();
    if (energyKwh < previous) {
        qCInfo(dcSunSpec()) << "Storage" << thing->name() << thing->thingClass().getStateType(stateTypeId).name()
                            << "went down from" << previous << "to" << energyKwh << "kWh: the device counter has been reset.";
    }

    thing->setStateValue(stateTypeId, energyKwh);
}

void IntegrationPluginSunSpec::updateStorageCapacity(Thing *thing)
{
    SunSpecNameplateModel *nameplate = m_storageNameplateModels.value(thing);
    if (!nameplate)
        return;

    const double capacity = SunSpecStorageLogic::nameplateCapacityKwh(nameplate->whRtg());
    if (capacity < 0) {
        /* The nameplate does not carry a rated storage capacity. Left at its
         * previous value rather than overwritten with a zero that would read
         * as a measurement. */
        qCDebug(dcSunSpec()) << "Storage" << thing->name() << "nameplate carries no WHRtg; capacity left untouched.";
        return;
    }

    thing->setStateValue(sunspecStorageCapacityStateTypeId, capacity);
}

void IntegrationPluginSunSpec::onStorageMpptBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    if (!model)
        return;

    /* Several storage things may share one model 160 - one connection, one
     * inverter, and in theory more than one storage block. */
    foreach (Thing *thing, m_storageMpptModels.keys(qobject_cast<SunSpecMpptModel *>(model)))
        updateStorageCurrentPower(thing);
}

void IntegrationPluginSunSpec::onStorageNameplateBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    if (!model)
        return;

    foreach (Thing *thing, m_storageNameplateModels.keys(qobject_cast<SunSpecNameplateModel *>(model)))
        updateStorageCapacity(thing);
}

void IntegrationPluginSunSpec::onSolarEdgeBatteryBlockUpdated()
{
    SolarEdgeBattery *battery = qobject_cast<SolarEdgeBattery *>(sender());
    Thing *thing = battery->thing();

    qCDebug(dcSunSpec()) << "SolarEdgeBattery: block data updated";// << battery->batteryData();

    thing->setStateValue(solarEdgeBatteryConnectedStateTypeId, true);

    QString chargingState = "idle";
    switch (battery->batteryData().batteryStatus) {
    case SolarEdgeBattery::Off:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Off");
        break;
    case SolarEdgeBattery::Standby:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Standby");
        break;
    case SolarEdgeBattery::Init:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Init");
        break;
    case SolarEdgeBattery::Charge:
        chargingState = "charging";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Charging");
        break;
    case SolarEdgeBattery::Discharge:
        chargingState = "discharging";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Discharging");
        break;
    case SolarEdgeBattery::Fault:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Fault");
        break;
    case SolarEdgeBattery::Holding:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Holding");
        break;
    case SolarEdgeBattery::Idle:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Idle");
        break;
    }

    thing->setStateValue(solarEdgeBatteryBatteryCriticalStateTypeId, (battery->batteryData().stateOfEnergy < 5) && chargingState != "charging");
    thing->setStateValue(solarEdgeBatteryBatteryLevelStateTypeId, battery->batteryData().stateOfEnergy);
    thing->setStateValue(solarEdgeBatteryChargingStateStateTypeId, chargingState);
    thing->setStateValue(solarEdgeBatteryRatedEnergyStateTypeId, battery->batteryData().ratedEnergy / 1000.0); // kWh
    thing->setStateValue(solarEdgeBatteryAverageTemperatureStateTypeId, battery->batteryData().averageTemperature);
    thing->setStateValue(solarEdgeBatteryInstantaneousVoltageStateTypeId, battery->batteryData().instantaneousVoltage);
    thing->setStateValue(solarEdgeBatteryInstantaneousCurrentStateTypeId, battery->batteryData().instantaneousCurrent);
    thing->setStateValue(solarEdgeBatteryCurrentPowerStateTypeId, battery->batteryData().instantaneousPower);
    thing->setStateValue(solarEdgeBatteryMaxEnergyStateTypeId, battery->batteryData().maxEnergy / 1000.0); // kWh
    thing->setStateValue(solarEdgeBatteryCapacityStateTypeId, battery->batteryData().availableEnergy / 1000.0); // kWh
    thing->setStateValue(solarEdgeBatteryStateOfHealthStateTypeId, battery->batteryData().stateOfHealth);
    thing->setStateValue(solarEdgeBatteryVersionStateTypeId, battery->batteryData().firmwareVersion);
}

void IntegrationPluginSunSpec::evaluateEnergyProducedValue(Thing *inverterThing, float energyProduced)
{
    /* Note: on some systems the inverter sends for a longer period an absurdly
     * high and wrong value for the produced energy (seen so far with SolarEdge inverters).
     *
     * In order to catch such situations, we need to verify if the state changed makes sense,
     * or if the difference is to big for a regular produced energy value.
     *
     * Following scenarios need to be considered:
     * - This is the first data value, we have no history to verify if this values makes sense
     * - The system might be switched off for some time, the energy produced could be much more than the last known value
     * - More than one value in a row could occure, not only single garbage data value
     */

    StateTypeId energyProducedStateTypeId;
    if (inverterThing->thingClassId() == sunspecSinglePhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecSinglePhaseInverterTotalEnergyProducedStateTypeId;
    } else if (inverterThing->thingClassId() == sunspecSplitPhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecSplitPhaseInverterTotalEnergyProducedStateTypeId;
    } else if (inverterThing->thingClassId() == sunspecThreePhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecThreePhaseInverterTotalEnergyProducedStateTypeId;
    } else {
        qCWarning(dcSunSpec()) << "Could not evaluate energy produced value for ThingClassId" << inverterThing->thingClassId() << "The value will not be updated.";
        return;
    }

    double currentEnergyValue = inverterThing->stateValue(energyProducedStateTypeId).toDouble();
    if (currentEnergyValue <= 0) {
        // Probably the initial value, no fancy data handling here
        inverterThing->setStateValue(energyProducedStateTypeId, energyProduced);
    } else {
        double producedDiff = energyProduced - currentEnergyValue;
        if (producedDiff > 10000 /*kWh*/) {
            // The new energy value is way to high in order to be a reglar energy produced change...
            qCWarning(dcSunSpec()) << "The energy produced value for" << inverterThing << "is way to high compared to the previouse value:"
                                   << currentEnergyValue << "kWh. Ignoring the value:" << energyProduced << "kWh.";
            return;
        } else {
            // Not a huge jump, just set the value
            inverterThing->setStateValue(energyProducedStateTypeId, energyProduced);
        }
    }
}
