// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2026, ETM-Schurig SARL
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

#ifndef NETENERGYINTEGRATOR_H
#define NETENERGYINTEGRATOR_H

#include <QtGlobal>

/*!
 * Builds net import / export indexes from the signed total power of a
 * three-phase meter (+ import, - export), the way a utility meter counts:
 * the three phases are summed BEFORE deciding the direction.
 *
 * Why: the SDM72D-M counts its energy registers (30073 import, 30075 export)
 * phase by phase. A phase carrying PV that exports while the two others draw
 * makes the export register climb although the house exports nothing in net.
 * The net register 30397 is only 30073 - 30075 and cannot tell, over a day,
 * the export at noon from the import in the evening.
 *
 * Pure logic, no nymea, no Modbus: the plugin feeds it and publishes it.
 */
class NetEnergyIntegrator
{
public:
    enum SampleResult {
        SampleNotSeeded,    // no starting index yet: nothing is counted
        SampleChainStarted, // first sample after seeding or after a break
        SampleIntegrated,   // interval since the previous sample counted
        SampleGapSkipped    // interval too long (or not monotonic): not counted
    };

    // Beyond this, the interval between two power reads is a hole, not a slow
    // read, and is NOT integrated: no energy is invented over a period nobody
    // read. Nominal step is the 2 s refresh timer; a cycle delayed by a timeout
    // and its retries still lands well under 10 s.
    static constexpr qint64 k_maxGapMs = 10000;

    bool isSeeded() const;

    // Starting point of both indexes. Also breaks the chain: the interval
    // between the last power read and the seed is never counted.
    void seed(double importKWh, double exportKWh);

    // Back to "not seeded". Used when net metering is switched off.
    void clear();

    // The next sample starts a new chain without integrating the interval
    // before it (meter unreachable, connection rebuilt).
    void breakChain();

    // powerW: signed total power (+ import, - export).
    // timestampMs: monotonic clock, in milliseconds.
    SampleResult addPowerSample(double powerW, qint64 timestampMs);

    double importKWh() const;
    double exportKWh() const;

    // True when the indexes moved since the last markSaved().
    bool hasUnsavedChanges() const;
    void markSaved();

    // Splits the trapezoid between two power samples into an import part and
    // an export part. When the sign changes in between, the zero crossing is
    // placed by linear interpolation, so that importKWh - exportKWh is always
    // exactly the trapezoid of the signed power.
    static void splitInterval(double fromW, double toW, qint64 durationMs,
                              double *importKWh, double *exportKWh);

private:
    bool m_seeded = false;
    bool m_hasPrevious = false;
    bool m_unsaved = false;
    double m_importKWh = 0;
    double m_exportKWh = 0;
    double m_previousW = 0;
    qint64 m_previousMs = 0;
};

#endif // NETENERGYINTEGRATOR_H
