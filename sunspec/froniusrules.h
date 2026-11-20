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

#ifndef FRONIUSRULES_H
#define FRONIUSRULES_H

#include <QList>
#include <QString>
#include <QStringList>

#include <sunspecdatapoint.h>

#include "sunspecstoragelogic.h"

/* Vendor specific rules for Fronius GEN24 inverters.
 *
 * Everything here is a pure decision: no Modbus, no thing, no timer. The
 * plugin asks these functions what to do; the answers are what the unit tests
 * pin down. See specs/fronius-sunspec.md.
 */
namespace FroniusRules {

/* Polling. The manufacturer's support recommends staying above seven seconds
 * for third party applications. Below that, communication load can push the
 * inverter offline towards the manufacturer's portal, and recovery needs an
 * AC+DC power cycle on site. This floor is a guard in the code, not a
 * configurable default. */
const int MinimumUpdateIntervalSeconds = 7;
const int DefaultUpdateIntervalSeconds = 10;
const int DefaultTimeoutMilliSeconds = 2000;
const int DefaultNumberOfRetries = 3;

/* Revert timer armed with every setpoint, so that a setpoint left behind by a
 * stopped or crashed HEMS expires instead of holding the battery forever.
 *
 * WORKING HYPOTHESIS: the timer semantics are documented in the manual's
 * general chapter and illustrated on model 123, not on model 124. This must be
 * verified experimentally on model 124 before being frozen. If it does not
 * hold, fall back to 0 plus an application level watchdog in the HEMS. */
const int DefaultRevertTimeSeconds = 300;

/* Default unit IDs. The inverter is fixed at 1 over TCP; the meter defaults to
 * 200, with 201, 202... for secondary meters. 0 means "no meter on this
 * connection". */
const int InverterSlaveId = 1;
const int DefaultMeterSlaveId = 200;
const int MaximumSlaveId = 247;

/* Whether a second SunSpecConnection should be opened for the Smart Meter, and
 * why not when it should not. The caller logs the reason; keeping it out of
 * this function is what makes the decision testable without nymea. */
enum MeterConnectionDecision {
    MeterConnectionCreate,
    /* 0 is the documented way of saying "no meter behind this inverter". */
    MeterConnectionDisabled,
    /* A second socket on the unit ID already served by the inverter would
     * rediscover the very same models and duplicate every child thing. */
    MeterConnectionCollidesWithInverter,
    MeterConnectionInvalidSlaveId
};

MeterConnectionDecision decideMeterConnection(int meterSlaveId, int inverterSlaveId);

bool matchesManufacturer(const QStringList &manufacturers);

/* Sign to apply to the raw SunSpec meter power registers (W, WphA...) to get
 * the nymea smartmeter convention, where positive is import and negative is
 * export.
 *
 * The Fronius Smart Meter already reports in that convention: negative W is
 * feed-in. The generic path negates, so on a Fronius the surplus came out with
 * the wrong sign and the arbiter refused to run loads while the site was
 * exporting.
 *
 * Established on hardware, not from the manufacturer's documentation. On a
 * Primo GEN24 3.0, a *single phase* inverter feeding phase A only, the plugin
 * reported phase A at +1003 W (import) while phases B and C read -33 W and
 * -262 W (export). Exporting on two phases carrying no source is impossible.
 * With the sign flipped the balance closes exactly: production 1190 W = phase A
 * load 187 W + phase A export 1003 W, B and C import 294 W, so the house draws
 * 481 W and the site exports 709 W against 708 W measured. The meter's own
 * energy counters agree: over the same window its import counter stayed at
 * zero while its export counter advanced.
 *
 * Only the instantaneous power is affected. The energy counters map correctly
 * already and must not be swapped.
 *
 * The generic default is left untouched: other vendors keep the negation. */
double meterPowerSign(bool froniusMeter);

/* Whether a model 124 block is backed by a physically installed battery.
 *
 * A hybrid GEN24 publishes model 124 whether or not a battery is wired to it,
 * so the presence of the model proves nothing. Creating a storage child anyway
 * gives the customer a phantom battery and a permanent batteryCritical alarm:
 * observed on hardware, a GEN24 with no battery reported ChaSt = Off and a
 * state of charge of 0, and nymea's own energy engine then logged "There is no
 * energy storage available to consider" while the child stayed visible.
 *
 * The discriminator is deliberately a *physical* quantity. The state of charge
 * is refused because 0% cannot be told apart from a real but empty battery, and
 * the setpoint registers (WChaMax and friends) are refused because they keep
 * their defaults with no battery attached.
 *
 * INFERRED: an installed battery carries a DC voltage at rest, so InBatV or
 * StorAval is expected to be strictly positive whenever one is present, and
 * both zero when none is. The absent side matches what the battery-less GEN24
 * showed and is reproduced on the simulator. The present side could NOT be
 * confirmed: no GEN24 with a battery was reachable. Confirm InBatV/StorAval on
 * a GEN24 with a battery before this ships. See DIVERGENCES.md. */
bool hasBattery(double batteryVoltage, double availableCharge);

/* Word order of the float32 points, measured on hardware.
 *
 * This used to read "INFERRED: not documented by the manufacturer, so the
 * SunSpec default is assumed. To be confirmed on a simulator, then on hardware."
 * It has now been confirmed, and the assumption was wrong: what was assumed to
 * be "the SunSpec default" was in fact libnymea-sunspec's own default, which is
 * the opposite of the specification.
 *
 * A float32 spans two registers. SunSpec puts the high word first.
 * SunSpecDataPoint::convertToUInt32 reads registers.at(0) then .at(1) for
 * ByteOrderBigEndian, and the reverse for ByteOrderLittleEndian — and little
 * endian is the library's default. Keeping that default on a Fronius means
 * reading every float32 with its two words swapped.
 *
 * Measured on a Fronius Symo, read only:
 *   meter 213, unit 200 : Hz     = 4248 0000 -> 50.0000 big endian
 *                         PhVphA = 436a 8000 -> 234.5000
 *   inverter 113, unit 1: Hz     = 4247 fc48 -> 49.9964
 * Swapped, those same registers give 2.4e-41 and -4.2e+36: denormals, the
 * signature of a wrong interpretation rather than a measurement.
 *
 * What it costs to get this wrong is not a small offset. Reproduced with a
 * simulator emitting the hardware's order: a meter importing
 * 2.7 kW was reported as exporting 107 MW, and the arbiter switched every load
 * on. Correcting the SIGN of a denormal changes nothing, so meterPowerSign()
 * only becomes observable once this is right.
 *
 * The SolarEdge vendor already forces big endian on its own path; the generic
 * vendor keeps its user-settable parameter. Neither is affected. */
SunSpecDataPoint::ByteOrder byteOrder();

int clampUpdateInterval(int requestedSeconds);

/* Registers the GEN24 accepts and then ignores without raising a Modbus
 * exception. Writing them fails invisibly, so they are refused before being
 * put on the wire. */
bool isWriteForbidden(const QString &registerName);

/* Whether a setpoint plan must be refused because it would put a limit in
 * place with the revert timer disabled.
 *
 * With InOutWRte_RvrtTms at 0 the inverter never drops the setpoint: a HEMS
 * that stops leaves the battery held by its last command, and nothing protects
 * it. Such a plan is refused, unless it only hands control back to the battery,
 * which must always be possible: refusing it would keep in place the very
 * limit the caller is trying to lift.
 *
 * A plan hands control back when every setpoint in it relaxes:
 *  - StorCtl_Mod sets no bit that currentStorCtlMod does not already carry
 *    (NORMAL, or one limit of two cleared);
 *  - InWRte / OutWRte go back to their full range, MaximumRatePercent;
 *  - ChaGriSet goes back to PV only.
 * The revert timer itself is not a setpoint. A plan with no timer write at all
 * counts as a disabled timer. */
bool isRefusedWithoutRevertTimer(const SunSpecStorageLogic::SetpointPlan &plan, int currentStorCtlMod);

enum BatteryMode {
    BatteryModeNormal,
    BatteryModeNoDischarge,
    BatteryModeNoCharge,
    BatteryModeFrozen,
    BatteryModeForceCharge,
    BatteryModeLimit
};

/* Sign convention of the manufacturer: negative is charging, positive is
 * discharging. chargeLimit and dischargeLimit are percentages of WChaMax and
 * are only used by the modes that need them. */
SunSpecStorageLogic::SetpointPlan planBatteryMode(BatteryMode mode,
                                                  double chargeLimit,
                                                  double dischargeLimit,
                                                  quint16 revertTime);

/* The setDischargeBlocked action: NO_DISCHARGE when blocked, NORMAL otherwise,
 * as one plan.
 *
 * Blocking with the enableDischarging and dischargingRate actions takes two
 * plans, and the inverter sits between them in an intermediate state. Here
 * StorCtl_Mod and OutWRte go out in one sequence, both are read back, and the
 * revert timer closes it. If the StorCtl_Mod write fails, nothing else is
 * written and the battery keeps its automatic mode.
 *
 * NORMAL writes StorCtl_Mod 0 first: the limit is released before OutWRte is
 * touched. */
SunSpecStorageLogic::SetpointPlan planDischargeBlock(bool blocked, quint16 revertTime);

/* Model 160 repeating blocks are identified by their IDStr, never by their
 * index: on a three or four MPPT inverter the battery blocks are not in
 * position 3 and 4. */
enum BlockRole {
    BlockRolePvString,
    BlockRoleBatteryCharge,
    BlockRoleBatteryDischarge,
    BlockRoleUnknown
};

BlockRole classifyBlock(const QString &inputIdString);

struct ModuleReading {
    QString inputIdString;

    /* Already scaled by DCW_SF, in watts. That factor is auto-scaled by the
     * inverter at runtime - measured moving from -2 to -1 between an idle and a
     * charging battery on the same machine - so it is only valid alongside the
     * powers of the same Modbus transaction. Whoever fills this struct owns
     * that guarantee; see the read order note above
     * IntegrationPluginSunSpec::storageModuleReadings(). */
    double dcPower;

    /* DCWH of the block, already scaled by DCWH_SF, in watt hours, or a
     * negative value when the block carries no publishable cumulative energy.
     * See moduleLifetimeEnergyWh(). Negative by default, so that a reading
     * built without it never passes for a measured zero. */
    double lifetimeEnergy = -1;
};

/* The DCWH point of one model 160 block, as the plugin may publish it: in watt
 * hours, or negative when it must not be published at all.
 *
 * `lifetimeEnergy` is what libnymea-sunspec hands out for the block, already
 * multiplied by 10^DCWH_SF and truncated to an integer. Three readings are not
 * energies:
 *
 *  - 0, the "not accumulated" value of a SunSpec acc32. It is also the initial
 *    value of the library before any valid read, and what a firmware before
 *    1.31 reports on the inactive battery block (Fronius GEN24 Modbus docs). Never
 *    a reset: a cumulative counter that is implemented does not come back to 0
 *    on its own;
 *  - a register full of ones, which is no acc32 sentinel but what an
 *    unimplemented 32-bit point looks like on most devices. After scaling, it
 *    ends up at or near 2^32, or saturated at 0xFFFFFFFF when the scaled value
 *    no longer fits;
 *  - anything read with an unimplemented DCWH_SF: an unscaled value is a guess
 *    of the order of magnitude.
 *
 * The library keeps the previous value of a point that reads invalid, so a
 * block that stops reporting leaves its last cumulative value in place rather
 * than a zero. That is the behaviour wanted here: a held total, never a
 * fabricated one. */
double moduleLifetimeEnergyWh(quint32 lifetimeEnergy, qint16 energyScaleFactor, bool scaleFactorImplemented);

/* Cumulative energy that went INTO the battery, in kWh: the sum of the DCWH of
 * the StCha blocks. Cumulative energy that came OUT of it: the sum over the
 * StDisCha blocks.
 *
 * Negative when there is nothing to publish: no block of that role, or one of
 * them without a publishable DCWH. A partial sum would be a total that jumps
 * the day the missing block starts reporting.
 *
 * These are DC side energies, measured between the battery and the inverter.
 * The AC side the house sees differs by the conversion losses of the inverter;
 * see DIVERGENCES.md. */
double storageChargedEnergyKwh(const QList<ModuleReading> &readings);
double storageDischargedEnergyKwh(const QList<ModuleReading> &readings);

/* Sum of the DC power of the PV string blocks. */
double pvPower(const QList<ModuleReading> &readings);

/* Discharge minus charge, so positive means discharging. DCW is a uint16 and
 * therefore always positive: the sign is carried by whichever block is active,
 * the inactive one reading zero.
 *
 * This is the MANUFACTURER convention, the one of AC-11 and AC-15. It is
 * the negation of storageCurrentPower() below, and is written as such so the
 * two can never drift apart. */
double batteryPower(const QList<ModuleReading> &readings);

/* Charge minus discharge, so positive means charging. This is the NYMEA
 * convention and the opposite of batteryPower() above.
 *
 * libnymea/interfaces/energystorage.json: "The currentPower state follows the
 * usual convention that positive is 'consumed' energy and negative values
 * represent 'produced' or in this case 'returned' energy." A battery that is
 * charging consumes, so charging is positive.
 *
 * Getting this backwards is not a cosmetic error: the storage reserve of the
 * arbiter would read a discharging battery as charging and free budget it does
 * not have. Measured on a test setup rather than deduced - see DIVERGENCES.md D-9. */
double storageCurrentPower(const QList<ModuleReading> &readings);

/* Whether the model 160 blocks carry a battery channel at all. A GEN24 with no
 * battery publishes N=2 (the two PV strings) and no StCha/StDisCha at all, so
 * the absence of both blocks is what "not measurable" looks like on the wire.
 *
 * The distinction matters because storageCurrentPower() answers 0 both for a
 * battery sitting idle and for an inverter that never reports one, and those
 * two zeros mean opposite things to a reserve rule. */
bool hasBatteryBlocks(const QList<ModuleReading> &readings);

/* Whether the blocks have been read at all, i.e. whether any of them carries an
 * ID string. This is NOT the same question as hasBatteryBlocks(), and confusing
 * the two produces a false alarm on every healthy start.
 *
 * libnymea-sunspec creates the repeating blocks of a model from `initFinished`,
 * which fires during the FIRST block read, just before `blockUpdated`. On that
 * first signal the blocks therefore exist but their data points have never been
 * filled: every inputIdSting() is empty, nothing classifies, and a caller that
 * only asked hasBatteryBlocks() would conclude "this inverter has no battery
 * channel" about a machine it has not read yet.
 *
 * Measured: the warning fired once at startup on a test setup whose StCha/StDisCha
 * blocks were present and whose currentPower worked from the next cycle on. */
bool blocksAreIdentified(const QList<ModuleReading> &readings);

}

#endif // FRONIUSRULES_H
