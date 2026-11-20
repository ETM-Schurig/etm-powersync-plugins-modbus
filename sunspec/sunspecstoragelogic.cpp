// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2013 - 2024, nymea GmbH
* Copyright (C) 2024 - 2025, chargebyte austria GmbH
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

#include "sunspecstoragelogic.h"

#include <QtMath>

namespace SunSpecStorageLogic {

StorageStates readStates(SunSpecStorageModel *storage)
{
    StorageStates states;
    if (!storage)
        return states;

    /* InWRte / OutWRte are what the chargingRate / dischargingRate actions
     * write. Reading the gradients here instead would mean the user writes one
     * quantity and reads back another. */
    states.chargingRate = storage->inWRte();
    states.dischargingRate = storage->outWRte();

    /* The gradients stay available, under names that cannot be mistaken for the
     * setpoints above. */
    states.chargeGradient = storage->wChaGra();
    states.dischargeGradient = storage->wDisChaGra();

    const SunSpecStorageModel::Storctl_modFlags controlMode = storage->storCtlMod();
    states.enableCharging = controlMode.testFlag(SunSpecStorageModel::Storctl_modCharge);
    states.enableDischarging = controlMode.testFlag(SunSpecStorageModel::Storctl_modDiScharge);

    states.gridCharging = (storage->chaGriSet() == SunSpecStorageModel::ChagrisetGrid);

    states.batteryLevel = qRound(storage->chaState());
    states.batteryCritical = storage->chaState() < 5;

    return states;
}

double nameplateCapacityKwh(double ratedEnergyWh)
{
    if (ratedEnergyWh <= 0)
        return -1;

    return ratedEnergyWh / 1000.0;
}

bool isScaledRate(Register reg)
{
    return reg == RegisterInWRte || reg == RegisterOutWRte;
}

QString registerName(Register reg)
{
    switch (reg) {
    case RegisterChaGriSet: return QStringLiteral("ChaGriSet");
    case RegisterStorCtlMod: return QStringLiteral("StorCtl_Mod");
    case RegisterInWRte: return QStringLiteral("InWRte");
    case RegisterOutWRte: return QStringLiteral("OutWRte");
    case RegisterRvrtTms: return QStringLiteral("InOutWRte_RvrtTms");
    }
    return QString();
}

/* Rank within the write order: ChaGriSet, then StorCtl_Mod, then the rates,
 * then the revert timer. Arming the timer first would start it counting against
 * a setpoint that has not been written yet. */
static int writeOrderRank(Register reg)
{
    switch (reg) {
    case RegisterChaGriSet: return 0;
    case RegisterStorCtlMod: return 1;
    case RegisterInWRte:
    case RegisterOutWRte: return 2;
    case RegisterRvrtTms: return 3;
    }
    return 3;
}

SetpointPlan planSetpoint(const QVector<Write> &setpoints, quint16 revertTime)
{
    SetpointPlan plan;

    QVector<Write> writes;
    foreach (const Write &write, setpoints) {
        /* The revert time is appended below, once, in last position. A caller
         * passing it explicitly must not end up with two of them. */
        if (write.reg == RegisterRvrtTms)
            continue;

        if (isScaledRate(write.reg))
            plan.rereadScaleFactor = true;

        writes.append(write);
    }

    std::stable_sort(writes.begin(), writes.end(), [](const Write &left, const Write &right) {
        return writeOrderRank(left.reg) < writeOrderRank(right.reg);
    });

    /* Every register that carries the intent is read back afterwards. The
     * revert timer is excluded: it is a countdown, so its read-back value is
     * not expected to match what was written. */
    foreach (const Write &write, writes) {
        if (!plan.verifyReadback.contains(write.reg))
            plan.verifyReadback.append(write.reg);
    }

    plan.writes = writes;
    plan.writes.append({RegisterRvrtTms, static_cast<double>(revertTime)});

    return plan;
}

}
