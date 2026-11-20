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

#include "netenergyintegrator.h"

static const double k_wattMsPerKWh = 3600000.0 * 1000.0;

bool NetEnergyIntegrator::isSeeded() const
{
    return m_seeded;
}

void NetEnergyIntegrator::seed(double importKWh, double exportKWh)
{
    m_importKWh = importKWh;
    m_exportKWh = exportKWh;
    m_seeded = true;
    m_hasPrevious = false;
    m_unsaved = true;
}

void NetEnergyIntegrator::clear()
{
    m_seeded = false;
    m_hasPrevious = false;
    m_unsaved = false;
    m_importKWh = 0;
    m_exportKWh = 0;
}

void NetEnergyIntegrator::breakChain()
{
    m_hasPrevious = false;
}

NetEnergyIntegrator::SampleResult NetEnergyIntegrator::addPowerSample(double powerW, qint64 timestampMs)
{
    if (!m_seeded)
        return SampleNotSeeded;

    if (!m_hasPrevious) {
        m_previousW = powerW;
        m_previousMs = timestampMs;
        m_hasPrevious = true;
        return SampleChainStarted;
    }

    const qint64 durationMs = timestampMs - m_previousMs;
    const double fromW = m_previousW;
    m_previousW = powerW;
    m_previousMs = timestampMs;

    if (durationMs <= 0 || durationMs > k_maxGapMs)
        return SampleGapSkipped;

    double importKWh = 0;
    double exportKWh = 0;
    splitInterval(fromW, powerW, durationMs, &importKWh, &exportKWh);
    if (importKWh > 0 || exportKWh > 0) {
        m_importKWh += importKWh;
        m_exportKWh += exportKWh;
        m_unsaved = true;
    }
    return SampleIntegrated;
}

double NetEnergyIntegrator::importKWh() const
{
    return m_importKWh;
}

double NetEnergyIntegrator::exportKWh() const
{
    return m_exportKWh;
}

bool NetEnergyIntegrator::hasUnsavedChanges() const
{
    return m_unsaved;
}

void NetEnergyIntegrator::markSaved()
{
    m_unsaved = false;
}

void NetEnergyIntegrator::splitInterval(double fromW, double toW, qint64 durationMs,
                                        double *importKWh, double *exportKWh)
{
    *importKWh = 0;
    *exportKWh = 0;
    if (durationMs <= 0)
        return;

    const double duration = static_cast<double>(durationMs);

    if (fromW >= 0 && toW >= 0) {
        *importKWh = (fromW + toW) / 2.0 * duration / k_wattMsPerKWh;
        return;
    }
    if (fromW <= 0 && toW <= 0) {
        *exportKWh = -(fromW + toW) / 2.0 * duration / k_wattMsPerKWh;
        return;
    }

    // Sign change: the zero crossing sits at fromW / (fromW - toW) of the interval.
    // Each side is a triangle.
    const double crossing = fromW / (fromW - toW);
    const double firstArea = fromW / 2.0 * crossing * duration;
    const double secondArea = toW / 2.0 * (1.0 - crossing) * duration;
    if (fromW > 0) {
        *importKWh = firstArea / k_wattMsPerKWh;
        *exportKWh = -secondArea / k_wattMsPerKWh;
    } else {
        *exportKWh = -firstArea / k_wattMsPerKWh;
        *importKWh = secondArea / k_wattMsPerKWh;
    }
}
