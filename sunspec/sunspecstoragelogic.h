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

#ifndef SUNSPECSTORAGELOGIC_H
#define SUNSPECSTORAGELOGIC_H

#include <QVector>

#include <models/sunspecstoragemodel.h>

/* Decision logic of the storage (model 124) handling, kept free of the
 * integration framework so it can be exercised without a running nymea.
 *
 * Two things live here:
 *
 *  - readStates() maps a storage block update onto the state values the plugin
 *    publishes. Having it in one place is what makes it possible to check that
 *    a state is fed from the very register the matching action writes to;
 *  - planSetpoint() turns a set of setpoints into an ordered sequence of
 *    register writes. Order matters on real hardware, and the revert timer
 *    must close every sequence.
 */
namespace SunSpecStorageLogic {

/* The registers of model 124 this plugin ever writes. */
enum Register {
    RegisterChaGriSet,
    RegisterStorCtlMod,
    RegisterInWRte,
    RegisterOutWRte,
    RegisterRvrtTms
};

struct Write {
    Register reg;
    double value;
};

struct SetpointPlan {
    /* Scale factors are not necessarily static: they may change with firmware
     * and at runtime. Whenever a scaled quantity is written, its scale factor
     * should be read again immediately before the write rather than reused
     * from an earlier poll. */
    bool rereadScaleFactor = false;

    /* Ordered: ChaGriSet, StorCtl_Mod, InWRte/OutWRte, RvrtTms last. */
    QVector<Write> writes;

    /* Registers to read back and compare after the sequence. Several inverters
     * accept a write and then ignore it without raising a Modbus exception, so
     * an unverified setpoint is an unapplied setpoint. */
    QVector<Register> verifyReadback;
};

struct StorageStates {
    /* Percent of WChaMax, from InWRte / OutWRte - the very registers the
     * chargingRate / dischargingRate actions write to. */
    double chargingRate = 0;
    double dischargingRate = 0;

    /* Percent of WChaMax per second, from WChaGra / WDisChaGra. A different
     * quantity from the two above, and read only on several inverters. */
    double chargeGradient = 0;
    double dischargeGradient = 0;

    /* Both flags are reconciled from StorCtl_Mod. Reconciling only one lets the
     * other drift silently as soon as the inverter changes it on its own. */
    bool enableCharging = false;
    bool enableDischarging = false;

    bool gridCharging = false;

    int batteryLevel = 0;
    bool batteryCritical = false;
};

/* Value range of the InWRte / OutWRte setpoints of model 124. */
const double MinimumRatePercent = -100.0;
const double MaximumRatePercent = 100.0;

/* Range of InOutWRte_RvrtTms. */
const int MaximumRevertTimeSeconds = 28800;

StorageStates readStates(SunSpecStorageModel *storage);

/* Nameplate rated storage capacity, Wh -> kWh, for the `capacity` state of the
 * energystorage interface.
 *
 * Model 124 carries no energy capacity: WChaMax is a power, StorAval is in amp
 * hours and reads zero on every machine measured so far. The rated capacity of
 * the storage lives in WHRtg of the nameplate model 120.
 *
 * Returns a NEGATIVE value when the nameplate carries no capacity, so that the
 * caller can leave the state alone instead of publishing a fabricated zero. A
 * nameplate that does not implement WHRtg reads 0xFFFF, which libnymea-sunspec
 * reports as an invalid data point and leaves at 0 - indistinguishable here
 * from a nameplate that genuinely says zero, and both mean "unknown". */
double nameplateCapacityKwh(double ratedEnergyWh);

/* True for the registers carried by InOutWRte_SF, i.e. those whose write should
 * be preceded by a fresh read of that scale factor. */
bool isScaledRate(Register reg);

QString registerName(Register reg);

SetpointPlan planSetpoint(const QVector<Write> &setpoints, quint16 revertTime);

}

#endif // SUNSPECSTORAGELOGIC_H
