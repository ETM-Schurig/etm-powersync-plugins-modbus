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

#include "froniusrules.h"

#include <QtMath>

using namespace SunSpecStorageLogic;

namespace FroniusRules {

bool matchesManufacturer(const QStringList &manufacturers)
{
    foreach (const QString &name, manufacturers) {
        if (name.contains(QStringLiteral("fronius"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

double meterPowerSign(bool froniusMeter)
{
    /* The Fronius meter is already in the nymea convention, so the generic
     * negation has to be undone for it, and only for it. */
    return froniusMeter ? 1.0 : -1.0;
}

bool hasBattery(double batteryVoltage, double availableCharge)
{
    /* Strictly positive only: zero is the no-battery reading, and a negative
     * value is the library's way of surfacing a not-implemented point rather
     * than a real measurement. */
    return batteryVoltage > 0.0 || availableCharge > 0.0;
}

SunSpecDataPoint::ByteOrder byteOrder()
{
    /* Big endian is what the hardware emits, and what SunSpec specifies. The
     * library's default is the opposite; see the header for the measurement. */
    return SunSpecDataPoint::ByteOrderBigEndian;
}

MeterConnectionDecision decideMeterConnection(int meterSlaveId, int inverterSlaveId)
{
    if (meterSlaveId == 0)
        return MeterConnectionDisabled;

    if (meterSlaveId < 0 || meterSlaveId > MaximumSlaveId)
        return MeterConnectionInvalidSlaveId;

    /* Not a theoretical case: the slave ID of the thing is written by the
     * discovery from the slave ID it was asked to scan, so an inverter found
     * with a discovery slave ID of 200 ends up with both values at 200. */
    if (meterSlaveId == inverterSlaveId)
        return MeterConnectionCollidesWithInverter;

    return MeterConnectionCreate;
}

int clampUpdateInterval(int requestedSeconds)
{
    return qMax(requestedSeconds, MinimumUpdateIntervalSeconds);
}

bool isWriteForbidden(const QString &registerName)
{
    static const QStringList forbidden = {
        QStringLiteral("WChaMax"),
        QStringLiteral("WChaGra"),
        QStringLiteral("WDisChaGra"),
        QStringLiteral("InOutWRte_WinTms"),
        QStringLiteral("InOutWRte_RmpTms"),
        /* VAChaMax is doubly unusable: the write is ignored and its scale
         * factor is not supported either. */
        QStringLiteral("VAChaMax")
    };

    foreach (const QString &name, forbidden) {
        if (registerName.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

static bool relaxes(const Write &write, int currentStorCtlMod)
{
    switch (write.reg) {
    case RegisterStorCtlMod:
        return (int(write.value) & ~currentStorCtlMod) == 0;
    case RegisterInWRte:
    case RegisterOutWRte:
        return write.value >= MaximumRatePercent;
    case RegisterChaGriSet:
        return write.value == double(SunSpecStorageModel::ChagrisetPv);
    case RegisterRvrtTms:
        return true;
    }
    return false;
}

bool isRefusedWithoutRevertTimer(const SetpointPlan &plan, int currentStorCtlMod)
{
    bool timerArmed = false;
    bool release = true;

    foreach (const Write &write, plan.writes) {
        if (write.reg == RegisterRvrtTms)
            timerArmed = write.value > 0;
        else if (!relaxes(write, currentStorCtlMod))
            release = false;
    }

    return !timerArmed && !release;
}

SetpointPlan planBatteryMode(BatteryMode mode, double chargeLimit, double dischargeLimit, quint16 revertTime)
{
    /* StorCtl_Mod is a bitfield: bit 0 CHARGE, bit 1 DISCHARGE. ChaGriSet is
     * 0 PV, 1 GRID. A register absent from the vector below is deliberately not
     * written for that mode.
     *
     * planSetpoint() puts these in the mandated order and appends the revert
     * timer, so the order they are listed in here does not matter. */
    QVector<Write> setpoints;

    switch (mode) {
    case BatteryModeNormal:
        setpoints.append({RegisterStorCtlMod, 0});
        setpoints.append({RegisterOutWRte, 100});
        break;

    case BatteryModeNoDischarge:
        setpoints.append({RegisterStorCtlMod, 2});
        setpoints.append({RegisterOutWRte, 0});
        break;

    case BatteryModeNoCharge:
        setpoints.append({RegisterStorCtlMod, 1});
        setpoints.append({RegisterInWRte, 0});
        break;

    case BatteryModeFrozen:
        setpoints.append({RegisterStorCtlMod, 3});
        setpoints.append({RegisterInWRte, 0});
        setpoints.append({RegisterOutWRte, 0});
        break;

    case BatteryModeForceCharge:
        /* Charging from the grid has to be allowed on the inverter as well;
         * ChaGriSet alone is ANDed with a setting of the component
         * configuration and fails silently if that one is off. */
        setpoints.append({RegisterChaGriSet, 1});
        setpoints.append({RegisterStorCtlMod, 2});
        /* Negative is charging. */
        setpoints.append({RegisterOutWRte, -chargeLimit});
        break;

    case BatteryModeLimit:
        setpoints.append({RegisterStorCtlMod, 3});
        setpoints.append({RegisterInWRte, chargeLimit});
        setpoints.append({RegisterOutWRte, dischargeLimit});
        break;
    }

    return planSetpoint(setpoints, revertTime);
}

SetpointPlan planDischargeBlock(bool blocked, quint16 revertTime)
{
    return planBatteryMode(blocked ? BatteryModeNoDischarge : BatteryModeNormal, 0, 0, revertTime);
}

BlockRole classifyBlock(const QString &inputIdString)
{
    const QString id = inputIdString.trimmed();
    if (id.isEmpty())
        return BlockRoleUnknown;

    /* Prefix match, not contains: the role is decided by the string, never by
     * the block index. StDisCha is checked first so that no cascade can ever
     * swallow it into the charge case. */
    if (id.startsWith(QStringLiteral("StDisCha"), Qt::CaseInsensitive))
        return BlockRoleBatteryDischarge;

    if (id.startsWith(QStringLiteral("StCha"), Qt::CaseInsensitive))
        return BlockRoleBatteryCharge;

    if (id.startsWith(QStringLiteral("MPPT"), Qt::CaseInsensitive))
        return BlockRolePvString;

    return BlockRoleUnknown;
}

double pvPower(const QList<ModuleReading> &readings)
{
    double total = 0;
    foreach (const ModuleReading &reading, readings) {
        if (classifyBlock(reading.inputIdString) == BlockRolePvString)
            total += reading.dcPower;
    }
    return total;
}

double storageCurrentPower(const QList<ModuleReading> &readings)
{
    double charging = 0;
    double discharging = 0;

    foreach (const ModuleReading &reading, readings) {
        switch (classifyBlock(reading.inputIdString)) {
        case BlockRoleBatteryCharge:
            charging += reading.dcPower;
            break;
        case BlockRoleBatteryDischarge:
            discharging += reading.dcPower;
            break;
        default:
            break;
        }
    }

    return charging - discharging;
}

double batteryPower(const QList<ModuleReading> &readings)
{
    /* Written as the negation rather than as a second sum: two independent
     * implementations of the same subtraction are two places for the sign to
     * rot, and only one of them has a test watching it. */
    return -storageCurrentPower(readings);
}

bool blocksAreIdentified(const QList<ModuleReading> &readings)
{
    foreach (const ModuleReading &reading, readings) {
        if (!reading.inputIdString.trimmed().isEmpty())
            return true;
    }
    return false;
}

bool hasBatteryBlocks(const QList<ModuleReading> &readings)
{
    foreach (const ModuleReading &reading, readings) {
        const BlockRole role = classifyBlock(reading.inputIdString);
        if (role == BlockRoleBatteryCharge || role == BlockRoleBatteryDischarge)
            return true;
    }
    return false;
}

double moduleLifetimeEnergyWh(quint32 lifetimeEnergy, qint16 energyScaleFactor, bool scaleFactorImplemented)
{
    if (!scaleFactorImplemented)
        return -1;

    if (lifetimeEnergy == 0 || lifetimeEnergy == 0xFFFFFFFF)
        return -1;

    /* Back to the register: a raw value in the top float step below 2^32 is a
     * register full of ones, whatever the scale factor did to it. */
    const double raw = lifetimeEnergy / qPow(10, energyScaleFactor);
    if (raw >= 4294967040.0)
        return -1;

    return lifetimeEnergy;
}

static double storageEnergyKwh(const QList<ModuleReading> &readings, BlockRole role)
{
    double total = 0;
    bool found = false;

    foreach (const ModuleReading &reading, readings) {
        if (classifyBlock(reading.inputIdString) != role)
            continue;

        if (reading.lifetimeEnergy < 0 || !qIsFinite(reading.lifetimeEnergy))
            return -1;

        total += reading.lifetimeEnergy;
        found = true;
    }

    return found ? total / 1000.0 : -1;
}

double storageChargedEnergyKwh(const QList<ModuleReading> &readings)
{
    return storageEnergyKwh(readings, BlockRoleBatteryCharge);
}

double storageDischargedEnergyKwh(const QList<ModuleReading> &readings)
{
    return storageEnergyKwh(readings, BlockRoleBatteryDischarge);
}

}
