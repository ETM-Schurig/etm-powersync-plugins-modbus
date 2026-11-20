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

#ifndef INTEGRATIONPLUGINEASTRON_H
#define INTEGRATIONPLUGINEASTRON_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "sdm630modbusrtuconnection.h"
#include "sdm72modbusrtuconnection.h"
#include "sdm120modbusrtuconnection.h"
#include "netenergyintegrator.h"

#include "extern-plugininfo.h"

#include <QObject>
#include <QHash>
#include <QElapsedTimer>

class IntegrationPluginEastron: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationplugineastron.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginEastron();
    ~IntegrationPluginEastron() override;
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    PluginTimer *m_refreshTimer = nullptr;

    // SDM630 group: sdm630, sdm630Consumer, sdm630Producer
    QHash<Thing *, Sdm630ModbusRtuConnection *> m_sdm630Connections;
    // SDM72 group: sdm72, sdm72Consumer, sdm72Producer
    QHash<Thing *, Sdm72ModbusRtuConnection *> m_sdm72Connections;
    // SDM120 group: sdm120/Consumer/Producer, sdm220/Consumer/Producer, sdm230/Consumer/Producer
    QHash<Thing *, Sdm120ModbusRtuConnection *> m_sdm120Connections;

    // SDM72 grid meter (sdm72 only): net import / export indexes built from the
    // signed total power, persisted in pluginStorage() under the thing id.
    PluginTimer *m_persistTimer = nullptr;
    QElapsedTimer m_monotonicClock;
    // Keyed by ThingId: at shutdown nymea deletes the things before the plugins,
    // and the destructor still has to save the indexes.
    QHash<ThingId, NetEnergyIntegrator> m_sdm72NetIntegrators;

    void discoverRtuDevices(ThingDiscoveryInfo *info, const QString &modelName,
                            const ParamTypeId &discoverySlaveParamTypeId,
                            const ParamTypeId &thingSlaveParamTypeId,
                            const ParamTypeId &thingMasterUuidParamTypeId);

    void setupSdm630(ThingSetupInfo *info);
    void setupSdm72(ThingSetupInfo *info);
    void setupSdm120(ThingSetupInfo *info);

    bool sdm72NetMetering(Thing *thing) const;
    void restoreSdm72NetIndexes(Thing *thing);
    void onSdm72PowerRead(Thing *thing, float powerW);
    void onSdm72EnergyRegistersRead(Thing *thing, float importKWh, float exportKWh);
    void onSdm72NetMeteringChanged(Thing *thing, bool enabled);
    void saveSdm72NetIndexes(const ThingId &thingId, bool force = false);
};

#endif // INTEGRATIONPLUGINEASTRON_H
