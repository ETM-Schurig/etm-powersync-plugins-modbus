// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2013 - 2024, nymea GmbH
* Copyright (C) 2025, ETM-Schurig SARL
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

#include "integrationplugineastron.h"
#include "plugininfo.h"

IntegrationPluginEastron::IntegrationPluginEastron()
{
    m_monotonicClock.start();
}

IntegrationPluginEastron::~IntegrationPluginEastron()
{
    // Clean shutdown: whatever was integrated since the last periodic save.
    foreach (const ThingId &thingId, m_sdm72NetIntegrators.keys()) {
        saveSdm72NetIndexes(thingId);
    }
}

void IntegrationPluginEastron::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved,
            this, [=](const QUuid &modbusUuid) {
        qCDebug(dcEastron()) << "Modbus RTU master removed:" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            ThingClassId classId = thing->thingClassId();

            if (classId == sdm630ThingClassId || classId == sdm630ConsumerThingClassId || classId == sdm630ProducerThingClassId) {
                if (thing->paramValue(sdm630ThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm630ConnectedStateTypeId, false);
                    delete m_sdm630Connections.take(thing);
                }
            } else if (classId == sdm630ConsumerThingClassId) {
                if (thing->paramValue(sdm630ConsumerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm630ConsumerConnectedStateTypeId, false);
                    delete m_sdm630Connections.take(thing);
                }
            } else if (classId == sdm630ProducerThingClassId) {
                if (thing->paramValue(sdm630ProducerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm630ProducerConnectedStateTypeId, false);
                    delete m_sdm630Connections.take(thing);
                }
            } else if (classId == sdm72ThingClassId) {
                if (thing->paramValue(sdm72ThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm72ConnectedStateTypeId, false);
                    delete m_sdm72Connections.take(thing);
                }
            } else if (classId == sdm72ConsumerThingClassId) {
                if (thing->paramValue(sdm72ConsumerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm72ConsumerConnectedStateTypeId, false);
                    delete m_sdm72Connections.take(thing);
                }
            } else if (classId == sdm72ProducerThingClassId) {
                if (thing->paramValue(sdm72ProducerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm72ProducerConnectedStateTypeId, false);
                    delete m_sdm72Connections.take(thing);
                }
            } else if (classId == sdm120ThingClassId) {
                if (thing->paramValue(sdm120ThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm120ConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm120ConsumerThingClassId) {
                if (thing->paramValue(sdm120ConsumerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm120ConsumerConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm120ProducerThingClassId) {
                if (thing->paramValue(sdm120ProducerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm120ProducerConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm220ThingClassId) {
                if (thing->paramValue(sdm220ThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm220ConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm220ConsumerThingClassId) {
                if (thing->paramValue(sdm220ConsumerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm220ConsumerConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm220ProducerThingClassId) {
                if (thing->paramValue(sdm220ProducerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm220ProducerConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm230ThingClassId) {
                if (thing->paramValue(sdm230ThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm230ConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm230ConsumerThingClassId) {
                if (thing->paramValue(sdm230ConsumerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm230ConsumerConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            } else if (classId == sdm230ProducerThingClassId) {
                if (thing->paramValue(sdm230ProducerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    thing->setStateValue(sdm230ProducerConnectedStateTypeId, false);
                    delete m_sdm120Connections.take(thing);
                }
            }
        }
    });
}

void IntegrationPluginEastron::discoverThings(ThingDiscoveryInfo *info)
{
    ThingClassId classId = info->thingClassId();
    QString modelName;
    ParamTypeId discoverySlaveParamTypeId;
    ParamTypeId thingSlaveParamTypeId;
    ParamTypeId thingMasterUuidParamTypeId;

    if (classId == sdm630ThingClassId) {
        modelName = "SDM630";
        discoverySlaveParamTypeId = sdm630DiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm630ThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm630ThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm630ConsumerThingClassId) {
        modelName = "SDM630";
        discoverySlaveParamTypeId = sdm630ConsumerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm630ConsumerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm630ConsumerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm630ProducerThingClassId) {
        modelName = "SDM630";
        discoverySlaveParamTypeId = sdm630ProducerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm630ProducerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm630ProducerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm72ThingClassId) {
        modelName = "SDM72";
        discoverySlaveParamTypeId = sdm72DiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm72ThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm72ThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm72ConsumerThingClassId) {
        modelName = "SDM72";
        discoverySlaveParamTypeId = sdm72ConsumerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm72ConsumerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm72ConsumerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm72ProducerThingClassId) {
        modelName = "SDM72";
        discoverySlaveParamTypeId = sdm72ProducerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm72ProducerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm72ProducerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm120ThingClassId) {
        modelName = "SDM120";
        discoverySlaveParamTypeId = sdm120DiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm120ThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm120ThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm120ConsumerThingClassId) {
        modelName = "SDM120";
        discoverySlaveParamTypeId = sdm120ConsumerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm120ConsumerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm120ConsumerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm120ProducerThingClassId) {
        modelName = "SDM120";
        discoverySlaveParamTypeId = sdm120ProducerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm120ProducerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm120ProducerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm220ThingClassId) {
        modelName = "SDM220";
        discoverySlaveParamTypeId = sdm220DiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm220ThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm220ThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm220ConsumerThingClassId) {
        modelName = "SDM220";
        discoverySlaveParamTypeId = sdm220ConsumerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm220ConsumerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm220ConsumerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm220ProducerThingClassId) {
        modelName = "SDM220";
        discoverySlaveParamTypeId = sdm220ProducerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm220ProducerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm220ProducerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm230ThingClassId) {
        modelName = "SDM230";
        discoverySlaveParamTypeId = sdm230DiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm230ThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm230ThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm230ConsumerThingClassId) {
        modelName = "SDM230";
        discoverySlaveParamTypeId = sdm230ConsumerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm230ConsumerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm230ConsumerThingModbusMasterUuidParamTypeId;
    } else if (classId == sdm230ProducerThingClassId) {
        modelName = "SDM230";
        discoverySlaveParamTypeId = sdm230ProducerDiscoverySlaveAddressParamTypeId;
        thingSlaveParamTypeId = sdm230ProducerThingSlaveAddressParamTypeId;
        thingMasterUuidParamTypeId = sdm230ProducerThingModbusMasterUuidParamTypeId;
    } else {
        info->finish(Thing::ThingErrorThingClassNotFound);
        return;
    }

    discoverRtuDevices(info, modelName, discoverySlaveParamTypeId, thingSlaveParamTypeId, thingMasterUuidParamTypeId);
}

void IntegrationPluginEastron::discoverRtuDevices(ThingDiscoveryInfo *info, const QString &modelName,
                                                   const ParamTypeId &discoverySlaveParamTypeId,
                                                   const ParamTypeId &thingSlaveParamTypeId,
                                                   const ParamTypeId &thingMasterUuidParamTypeId)
{
    if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
        info->finish(Thing::ThingErrorHardwareNotAvailable,
                     QT_TR_NOOP("No Modbus RTU interface available. Please set up the Modbus RTU interface first."));
        return;
    }

    uint slaveAddress = info->params().paramValue(discoverySlaveParamTypeId).toUInt();
    if (slaveAddress == 0 || slaveAddress > 254) {
        info->finish(Thing::ThingErrorInvalidParameter,
                     QT_TR_NOOP("The Modbus slave address must be a value between 1 and 254."));
        return;
    }

    foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
        qCDebug(dcEastron()) << "Found RTU master" << modbusMaster << "connected:" << modbusMaster->connected();
        if (!modbusMaster->connected())
            continue;

        ThingDescriptor descriptor(info->thingClassId(), modelName,
                                   QString::number(slaveAddress) + " " + modbusMaster->serialPort());
        ParamList params;
        params << Param(thingSlaveParamTypeId, slaveAddress);
        params << Param(thingMasterUuidParamTypeId, modbusMaster->modbusUuid());
        descriptor.setParams(params);
        info->addThingDescriptor(descriptor);
    }

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginEastron::setupThing(ThingSetupInfo *info)
{
    ThingClassId classId = info->thing()->thingClassId();

    if (classId == sdm630ThingClassId || classId == sdm630ConsumerThingClassId || classId == sdm630ProducerThingClassId) {
        setupSdm630(info);
    } else if (classId == sdm72ThingClassId || classId == sdm72ConsumerThingClassId || classId == sdm72ProducerThingClassId) {
        setupSdm72(info);
    } else if (classId == sdm120ThingClassId || classId == sdm120ConsumerThingClassId || classId == sdm120ProducerThingClassId
               || classId == sdm220ThingClassId || classId == sdm220ConsumerThingClassId || classId == sdm220ProducerThingClassId
               || classId == sdm230ThingClassId || classId == sdm230ConsumerThingClassId || classId == sdm230ProducerThingClassId) {
        setupSdm120(info);
    } else {
        info->finish(Thing::ThingErrorThingClassNotFound);
    }
}

void IntegrationPluginEastron::setupSdm630(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    ThingClassId classId = thing->thingClassId();

    ParamTypeId slaveParamTypeId;
    ParamTypeId masterUuidParamTypeId;
    StateTypeId connectedStateTypeId;

    if (classId == sdm630ThingClassId) {
        slaveParamTypeId = sdm630ThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm630ThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm630ConnectedStateTypeId;
    } else if (classId == sdm630ConsumerThingClassId) {
        slaveParamTypeId = sdm630ConsumerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm630ConsumerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm630ConsumerConnectedStateTypeId;
    } else {
        slaveParamTypeId = sdm630ProducerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm630ProducerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm630ProducerConnectedStateTypeId;
    }

    uint address = thing->paramValue(slaveParamTypeId).toUInt();
    if (address == 0 || address > 254) {
        qCWarning(dcEastron()) << "Setup failed, invalid slave address" << address;
        info->finish(Thing::ThingErrorSetupFailed,
                     QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
        return;
    }

    QUuid uuid = thing->paramValue(masterUuidParamTypeId).toUuid();
    if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
        qCWarning(dcEastron()) << "Setup failed, Modbus RTU master not available";
        info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
        return;
    }

    if (m_sdm630Connections.contains(thing))
        m_sdm630Connections.take(thing)->deleteLater();

    Sdm630ModbusRtuConnection *connection = new Sdm630ModbusRtuConnection(
        hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);

    connect(connection, &Sdm630ModbusRtuConnection::reachableChanged, this, [=](bool reachable) {
        thing->setStateValue(connectedStateTypeId, reachable);
    });

    if (classId == sdm630ThingClassId) {
        connect(connection, &Sdm630ModbusRtuConnection::voltagePhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm630VoltagePhaseAStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::voltagePhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm630VoltagePhaseBStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::voltagePhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm630VoltagePhaseCStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::currentPhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm630CurrentPhaseAStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::currentPhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm630CurrentPhaseBStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::currentPhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm630CurrentPhaseCStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float v) {
            thing->setStateValue(sdm630CurrentPowerStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::powerPhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm630CurrentPowerPhaseAStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::powerPhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm630CurrentPowerPhaseBStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::powerPhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm630CurrentPowerPhaseCStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::frequencyChanged, this, [=](float v) {
            thing->setStateValue(sdm630FrequencyStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float v) {
            thing->setStateValue(sdm630TotalEnergyConsumedStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::totalEnergyProducedChanged, this, [=](float v) {
            thing->setStateValue(sdm630TotalEnergyProducedStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::energyConsumedPhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm630EnergyConsumedPhaseAStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::energyConsumedPhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm630EnergyConsumedPhaseBStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::energyConsumedPhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm630EnergyConsumedPhaseCStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::energyProducedPhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm630EnergyProducedPhaseAStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::energyProducedPhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm630EnergyProducedPhaseBStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::energyProducedPhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm630EnergyProducedPhaseCStateTypeId, v);
        });

    } else if (classId == sdm630ConsumerThingClassId) {
        connect(connection, &Sdm630ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float v) {
            thing->setStateValue(sdm630ConsumerCurrentPowerStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float v) {
            thing->setStateValue(sdm630ConsumerTotalEnergyConsumedStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::frequencyChanged, this, [=](float v) {
            thing->setStateValue(sdm630ConsumerFrequencyStateTypeId, v);
        });

    } else { // sdm630Producer
        connect(connection, &Sdm630ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float v) {
            thing->setStateValue(sdm630ProducerCurrentPowerStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::totalEnergyProducedChanged, this, [=](float v) {
            thing->setStateValue(sdm630ProducerTotalEnergyProducedStateTypeId, v);
        });
        connect(connection, &Sdm630ModbusRtuConnection::frequencyChanged, this, [=](float v) {
            thing->setStateValue(sdm630ProducerFrequencyStateTypeId, v);
        });
    }

    m_sdm630Connections.insert(thing, connection);
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginEastron::setupSdm72(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    ThingClassId classId = thing->thingClassId();

    ParamTypeId slaveParamTypeId;
    ParamTypeId masterUuidParamTypeId;
    StateTypeId connectedStateTypeId;

    if (classId == sdm72ThingClassId) {
        slaveParamTypeId = sdm72ThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm72ThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm72ConnectedStateTypeId;
    } else if (classId == sdm72ConsumerThingClassId) {
        slaveParamTypeId = sdm72ConsumerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm72ConsumerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm72ConsumerConnectedStateTypeId;
    } else {
        slaveParamTypeId = sdm72ProducerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm72ProducerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm72ProducerConnectedStateTypeId;
    }

    uint address = thing->paramValue(slaveParamTypeId).toUInt();
    if (address == 0 || address > 254) {
        qCWarning(dcEastron()) << "Setup failed, invalid slave address" << address;
        info->finish(Thing::ThingErrorSetupFailed,
                     QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
        return;
    }

    QUuid uuid = thing->paramValue(masterUuidParamTypeId).toUuid();
    if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
        qCWarning(dcEastron()) << "Setup failed, Modbus RTU master not available";
        info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
        return;
    }

    if (m_sdm72Connections.contains(thing))
        m_sdm72Connections.take(thing)->deleteLater();

    Sdm72ModbusRtuConnection *connection = new Sdm72ModbusRtuConnection(
        hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);

    connect(connection, &Sdm72ModbusRtuConnection::reachableChanged, this, [=](bool reachable) {
        thing->setStateValue(connectedStateTypeId, reachable);
        // Never integrate across a period where the meter was not read.
        if (!reachable && m_sdm72NetIntegrators.contains(thing->id()))
            m_sdm72NetIntegrators[thing->id()].breakChain();
    });

    if (classId == sdm72ThingClassId) {
        connect(connection, &Sdm72ModbusRtuConnection::voltagePhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm72VoltagePhaseAStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::voltagePhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm72VoltagePhaseBStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::voltagePhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm72VoltagePhaseCStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::currentPhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm72CurrentPhaseAStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::currentPhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm72CurrentPhaseBStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::currentPhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm72CurrentPhaseCStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float v) {
            thing->setStateValue(sdm72CurrentPowerStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::powerPhaseAChanged, this, [=](float v) {
            thing->setStateValue(sdm72CurrentPowerPhaseAStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::powerPhaseBChanged, this, [=](float v) {
            thing->setStateValue(sdm72CurrentPowerPhaseBStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::powerPhaseCChanged, this, [=](float v) {
            thing->setStateValue(sdm72CurrentPowerPhaseCStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::frequencyChanged, this, [=](float v) {
            thing->setStateValue(sdm72FrequencyStateTypeId, v);
        });

        // totalEnergyConsumed / totalEnergyProduced: net indexes integrated from the
        // signed total power (default), or the per-phase registers 30073 / 30075 when
        // net metering is off. The registers stay readable in their own states.
        // ReadFinished, not Changed: every read is a sample, even an unchanged one.
        connect(connection, &Sdm72ModbusRtuConnection::totalCurrentPowerReadFinished, this, [=](float v) {
            onSdm72PowerRead(thing, v);
        });
        // 30073 is processed before 30075 in the same block reply.
        connect(connection, &Sdm72ModbusRtuConnection::totalEnergyProducedReadFinished, this, [=](float v) {
            onSdm72EnergyRegistersRead(thing, connection->totalEnergyConsumed(), v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::netActiveEnergyChanged, this, [=](float v) {
            thing->setStateValue(sdm72NetEnergyRegisterStateTypeId, v);
        });
        // Context = connection: a re-setup must not stack a second handler on the thing.
        connect(thing, &Thing::settingChanged, connection, [=](const ParamTypeId &paramTypeId, const QVariant &value) {
            if (paramTypeId == sdm72SettingsNetMeteringParamTypeId)
                onSdm72NetMeteringChanged(thing, value.toBool());
        });
        restoreSdm72NetIndexes(thing);

    } else if (classId == sdm72ConsumerThingClassId) {
        connect(connection, &Sdm72ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float v) {
            thing->setStateValue(sdm72ConsumerCurrentPowerStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float v) {
            thing->setStateValue(sdm72ConsumerTotalEnergyConsumedStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::frequencyChanged, this, [=](float v) {
            thing->setStateValue(sdm72ConsumerFrequencyStateTypeId, v);
        });

    } else { // sdm72Producer
        connect(connection, &Sdm72ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float v) {
            thing->setStateValue(sdm72ProducerCurrentPowerStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::totalEnergyProducedChanged, this, [=](float v) {
            thing->setStateValue(sdm72ProducerTotalEnergyProducedStateTypeId, v);
        });
        connect(connection, &Sdm72ModbusRtuConnection::frequencyChanged, this, [=](float v) {
            thing->setStateValue(sdm72ProducerFrequencyStateTypeId, v);
        });
    }

    m_sdm72Connections.insert(thing, connection);
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginEastron::setupSdm120(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    ThingClassId classId = thing->thingClassId();

    ParamTypeId slaveParamTypeId;
    ParamTypeId masterUuidParamTypeId;
    StateTypeId connectedStateTypeId;
    StateTypeId voltageStateTypeId;
    StateTypeId currentStateTypeId;
    StateTypeId powerStateTypeId;
    StateTypeId frequencyStateTypeId;
    StateTypeId totalEnergyConsumedStateTypeId;
    StateTypeId totalEnergyProducedStateTypeId;
    bool isEnergyMeter = false;

    if (classId == sdm120ThingClassId) {
        slaveParamTypeId = sdm120ThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm120ThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm120ConnectedStateTypeId;
        voltageStateTypeId = sdm120VoltagePhaseAStateTypeId;
        currentStateTypeId = sdm120CurrentPhaseAStateTypeId;
        powerStateTypeId = sdm120CurrentPowerStateTypeId;
        frequencyStateTypeId = sdm120FrequencyStateTypeId;
        totalEnergyConsumedStateTypeId = sdm120TotalEnergyConsumedStateTypeId;
        totalEnergyProducedStateTypeId = sdm120TotalEnergyProducedStateTypeId;
        isEnergyMeter = true;
    } else if (classId == sdm120ConsumerThingClassId) {
        slaveParamTypeId = sdm120ConsumerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm120ConsumerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm120ConsumerConnectedStateTypeId;
        powerStateTypeId = sdm120ConsumerCurrentPowerStateTypeId;
        frequencyStateTypeId = sdm120ConsumerFrequencyStateTypeId;
        totalEnergyConsumedStateTypeId = sdm120ConsumerTotalEnergyConsumedStateTypeId;
    } else if (classId == sdm120ProducerThingClassId) {
        slaveParamTypeId = sdm120ProducerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm120ProducerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm120ProducerConnectedStateTypeId;
        powerStateTypeId = sdm120ProducerCurrentPowerStateTypeId;
        frequencyStateTypeId = sdm120ProducerFrequencyStateTypeId;
        totalEnergyProducedStateTypeId = sdm120ProducerTotalEnergyProducedStateTypeId;
    } else if (classId == sdm220ThingClassId) {
        slaveParamTypeId = sdm220ThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm220ThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm220ConnectedStateTypeId;
        voltageStateTypeId = sdm220VoltagePhaseAStateTypeId;
        currentStateTypeId = sdm220CurrentPhaseAStateTypeId;
        powerStateTypeId = sdm220CurrentPowerStateTypeId;
        frequencyStateTypeId = sdm220FrequencyStateTypeId;
        totalEnergyConsumedStateTypeId = sdm220TotalEnergyConsumedStateTypeId;
        totalEnergyProducedStateTypeId = sdm220TotalEnergyProducedStateTypeId;
        isEnergyMeter = true;
    } else if (classId == sdm220ConsumerThingClassId) {
        slaveParamTypeId = sdm220ConsumerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm220ConsumerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm220ConsumerConnectedStateTypeId;
        powerStateTypeId = sdm220ConsumerCurrentPowerStateTypeId;
        frequencyStateTypeId = sdm220ConsumerFrequencyStateTypeId;
        totalEnergyConsumedStateTypeId = sdm220ConsumerTotalEnergyConsumedStateTypeId;
    } else if (classId == sdm220ProducerThingClassId) {
        slaveParamTypeId = sdm220ProducerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm220ProducerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm220ProducerConnectedStateTypeId;
        powerStateTypeId = sdm220ProducerCurrentPowerStateTypeId;
        frequencyStateTypeId = sdm220ProducerFrequencyStateTypeId;
        totalEnergyProducedStateTypeId = sdm220ProducerTotalEnergyProducedStateTypeId;
    } else if (classId == sdm230ThingClassId) {
        slaveParamTypeId = sdm230ThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm230ThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm230ConnectedStateTypeId;
        voltageStateTypeId = sdm230VoltagePhaseAStateTypeId;
        currentStateTypeId = sdm230CurrentPhaseAStateTypeId;
        powerStateTypeId = sdm230CurrentPowerStateTypeId;
        frequencyStateTypeId = sdm230FrequencyStateTypeId;
        totalEnergyConsumedStateTypeId = sdm230TotalEnergyConsumedStateTypeId;
        totalEnergyProducedStateTypeId = sdm230TotalEnergyProducedStateTypeId;
        isEnergyMeter = true;
    } else if (classId == sdm230ConsumerThingClassId) {
        slaveParamTypeId = sdm230ConsumerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm230ConsumerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm230ConsumerConnectedStateTypeId;
        powerStateTypeId = sdm230ConsumerCurrentPowerStateTypeId;
        frequencyStateTypeId = sdm230ConsumerFrequencyStateTypeId;
        totalEnergyConsumedStateTypeId = sdm230ConsumerTotalEnergyConsumedStateTypeId;
    } else { // sdm230Producer
        slaveParamTypeId = sdm230ProducerThingSlaveAddressParamTypeId;
        masterUuidParamTypeId = sdm230ProducerThingModbusMasterUuidParamTypeId;
        connectedStateTypeId = sdm230ProducerConnectedStateTypeId;
        powerStateTypeId = sdm230ProducerCurrentPowerStateTypeId;
        frequencyStateTypeId = sdm230ProducerFrequencyStateTypeId;
        totalEnergyProducedStateTypeId = sdm230ProducerTotalEnergyProducedStateTypeId;
    }

    uint address = thing->paramValue(slaveParamTypeId).toUInt();
    if (address == 0 || address > 254) {
        qCWarning(dcEastron()) << "Setup failed, invalid slave address" << address;
        info->finish(Thing::ThingErrorSetupFailed,
                     QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
        return;
    }

    QUuid uuid = thing->paramValue(masterUuidParamTypeId).toUuid();
    if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
        qCWarning(dcEastron()) << "Setup failed, Modbus RTU master not available";
        info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
        return;
    }

    if (m_sdm120Connections.contains(thing))
        m_sdm120Connections.take(thing)->deleteLater();

    Sdm120ModbusRtuConnection *connection = new Sdm120ModbusRtuConnection(
        hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);

    connect(connection, &Sdm120ModbusRtuConnection::reachableChanged, this, [=](bool reachable) {
        thing->setStateValue(connectedStateTypeId, reachable);
    });

    connect(connection, &Sdm120ModbusRtuConnection::activePowerChanged, this, [=](float v) {
        thing->setStateValue(powerStateTypeId, v);
    });
    connect(connection, &Sdm120ModbusRtuConnection::frequencyChanged, this, [=](float v) {
        thing->setStateValue(frequencyStateTypeId, v);
    });

    if (isEnergyMeter) {
        connect(connection, &Sdm120ModbusRtuConnection::voltageChanged, this, [=](float v) {
            thing->setStateValue(voltageStateTypeId, v);
        });
        connect(connection, &Sdm120ModbusRtuConnection::currentChanged, this, [=](float v) {
            thing->setStateValue(currentStateTypeId, v);
        });
        connect(connection, &Sdm120ModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float v) {
            thing->setStateValue(totalEnergyConsumedStateTypeId, v);
        });
        connect(connection, &Sdm120ModbusRtuConnection::totalEnergyProducedChanged, this, [=](float v) {
            thing->setStateValue(totalEnergyProducedStateTypeId, v);
        });
    } else {
        // Consumer or producer: connect only the relevant energy direction
        bool isProducer = (classId == sdm120ProducerThingClassId
                           || classId == sdm220ProducerThingClassId
                           || classId == sdm230ProducerThingClassId);
        if (isProducer) {
            connect(connection, &Sdm120ModbusRtuConnection::totalEnergyProducedChanged, this, [=](float v) {
                thing->setStateValue(totalEnergyProducedStateTypeId, v);
            });
        } else {
            connect(connection, &Sdm120ModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float v) {
                thing->setStateValue(totalEnergyConsumedStateTypeId, v);
            });
        }
    }

    m_sdm120Connections.insert(thing, connection);
    info->finish(Thing::ThingErrorNoError);
}

bool IntegrationPluginEastron::sdm72NetMetering(Thing *thing) const
{
    return thing->setting(sdm72SettingsNetMeteringParamTypeId).toBool();
}

void IntegrationPluginEastron::restoreSdm72NetIndexes(Thing *thing)
{
    // Re-setup of a running thing: the indexes in memory are the most recent ones.
    if (m_sdm72NetIntegrators.contains(thing->id())) {
        m_sdm72NetIntegrators[thing->id()].breakChain();
        return;
    }

    QSettings *storage = pluginStorage();
    if (!sdm72NetMetering(thing)) {
        storage->remove(thing->id().toString());
        return;
    }

    // No saved indexes (first activation, or update from a version that published
    // the registers): they will be seeded on 30073 / 30075 at the first read.
    storage->beginGroup(thing->id().toString());
    if (storage->contains("netImportKWh") && storage->contains("netExportKWh")) {
        NetEnergyIntegrator integrator;
        integrator.seed(storage->value("netImportKWh").toDouble(), storage->value("netExportKWh").toDouble());
        integrator.markSaved();
        m_sdm72NetIntegrators.insert(thing->id(), integrator);
        qCInfo(dcEastron()) << thing->name() << "net indexes restored: import" << integrator.importKWh()
                            << "kWh, export" << integrator.exportKWh() << "kWh";
    }
    storage->endGroup();
}

void IntegrationPluginEastron::onSdm72PowerRead(Thing *thing, float powerW)
{
    if (!sdm72NetMetering(thing) || !m_sdm72NetIntegrators.contains(thing->id()))
        return;

    NetEnergyIntegrator &integrator = m_sdm72NetIntegrators[thing->id()];
    switch (integrator.addPowerSample(powerW, m_monotonicClock.elapsed())) {
    case NetEnergyIntegrator::SampleIntegrated:
        thing->setStateValue(sdm72TotalEnergyConsumedStateTypeId, integrator.importKWh());
        thing->setStateValue(sdm72TotalEnergyProducedStateTypeId, integrator.exportKWh());
        break;
    case NetEnergyIntegrator::SampleGapSkipped:
        qCInfo(dcEastron()) << thing->name() << "more than" << NetEnergyIntegrator::k_maxGapMs
                            << "ms since the previous power read: interval not integrated";
        break;
    default:
        break;
    }
}

void IntegrationPluginEastron::onSdm72EnergyRegistersRead(Thing *thing, float importKWh, float exportKWh)
{
    thing->setStateValue(sdm72ImportEnergyRegisterStateTypeId, importKWh);
    thing->setStateValue(sdm72ExportEnergyRegisterStateTypeId, exportKWh);

    if (!sdm72NetMetering(thing)) {
        thing->setStateValue(sdm72TotalEnergyConsumedStateTypeId, importKWh);
        thing->setStateValue(sdm72TotalEnergyProducedStateTypeId, exportKWh);
        return;
    }

    if (m_sdm72NetIntegrators.contains(thing->id()))
        return;

    // Seeded on the registers, i.e. on the values published until now: the energy
    // experience adds every positive step of these states, so the indexes must not
    // start above what it last saw.
    NetEnergyIntegrator integrator;
    integrator.seed(importKWh, exportKWh);
    m_sdm72NetIntegrators.insert(thing->id(), integrator);
    thing->setStateValue(sdm72TotalEnergyConsumedStateTypeId, integrator.importKWh());
    thing->setStateValue(sdm72TotalEnergyProducedStateTypeId, integrator.exportKWh());
    saveSdm72NetIndexes(thing->id(), true);
    qCInfo(dcEastron()) << thing->name() << "net metering: indexes seeded on registers 30073 / 30075: import"
                        << importKWh << "kWh, export" << exportKWh << "kWh";
}

void IntegrationPluginEastron::onSdm72NetMeteringChanged(Thing *thing, bool enabled)
{
    m_sdm72NetIntegrators.remove(thing->id());
    pluginStorage()->remove(thing->id().toString());
    pluginStorage()->sync();

    if (enabled) {
        qCInfo(dcEastron()) << thing->name() << "net metering on: indexes will start from registers 30073 / 30075 at the next read";
    } else {
        // The registers count phase by phase and are never below the net indexes:
        // the energy experience will count the difference as one step.
        qCWarning(dcEastron()) << thing->name() << "net metering off: publishing the per-phase registers 30073 / 30075 again."
                               << "The difference with the net indexes will appear as a step in the energy totals.";
    }
}

void IntegrationPluginEastron::saveSdm72NetIndexes(const ThingId &thingId, bool force)
{
    if (!m_sdm72NetIntegrators.contains(thingId))
        return;

    NetEnergyIntegrator &integrator = m_sdm72NetIntegrators[thingId];
    if (!integrator.isSeeded() || (!force && !integrator.hasUnsavedChanges()))
        return;

    QSettings *storage = pluginStorage();
    storage->beginGroup(thingId.toString());
    storage->setValue("netImportKWh", integrator.importKWh());
    storage->setValue("netExportKWh", integrator.exportKWh());
    storage->endGroup();
    storage->sync();
    integrator.markSaved();
}

void IntegrationPluginEastron::postSetupThing(Thing *thing)
{
    qCDebug(dcEastron()) << "Post setup thing" << thing->name();
    if (!m_refreshTimer) {
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach (Thing *thing, myThings()) {
                if (m_sdm630Connections.contains(thing))
                    m_sdm630Connections.value(thing)->update();
                else if (m_sdm72Connections.contains(thing))
                    m_sdm72Connections.value(thing)->update();
                else if (m_sdm120Connections.contains(thing))
                    m_sdm120Connections.value(thing)->update();
            }
        });
        qCDebug(dcEastron()) << "Refresh timer started";
        m_refreshTimer->start();
    }

    // Net indexes are saved once a minute, not at every read: the storage is on the
    // gateway's flash. A crash loses at most one minute of integration, and the index
    // then steps back, which the energy experience treats as a reset (re-sync, no step).
    if (!m_persistTimer) {
        m_persistTimer = hardwareManager()->pluginTimerManager()->registerTimer(60);
        connect(m_persistTimer, &PluginTimer::timeout, this, [this] {
            foreach (Thing *thing, myThings()) {
                saveSdm72NetIndexes(thing->id());
            }
        });
        m_persistTimer->start();
    }
}

void IntegrationPluginEastron::thingRemoved(Thing *thing)
{
    qCDebug(dcEastron()) << "Thing removed" << thing->name();

    if (m_sdm630Connections.contains(thing))
        m_sdm630Connections.take(thing)->deleteLater();
    else if (m_sdm72Connections.contains(thing))
        m_sdm72Connections.take(thing)->deleteLater();
    else if (m_sdm120Connections.contains(thing))
        m_sdm120Connections.take(thing)->deleteLater();

    // Indexes without an owner: a meter paired again starts from its registers.
    m_sdm72NetIntegrators.remove(thing->id());
    pluginStorage()->remove(thing->id().toString());

    if (myThings().isEmpty() && m_refreshTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
        qCDebug(dcEastron()) << "Refresh timer stopped";
    }
    if (myThings().isEmpty() && m_persistTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_persistTimer);
        m_persistTimer = nullptr;
    }
}
