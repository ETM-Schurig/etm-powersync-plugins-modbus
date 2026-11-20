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

#include "sunspecduplicates.h"

#include <models/sunspecmodelfactory.h>

namespace SunSpecDuplicates {

ModelFamily modelFamily(quint16 modelId)
{
    switch (modelId) {
    case SunSpecModelFactory::ModelIdInverterSinglePhase:
    case SunSpecModelFactory::ModelIdInverterSplitPhase:
    case SunSpecModelFactory::ModelIdInverterThreePhase:
    case SunSpecModelFactory::ModelIdInverterSinglePhaseFloat:
    case SunSpecModelFactory::ModelIdInverterSplitPhaseFloat:
    case SunSpecModelFactory::ModelIdInverterThreePhaseFloat:
        return ModelFamilyInverter;
    case SunSpecModelFactory::ModelIdMeterSinglePhase:
    case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseAbn:
    case SunSpecModelFactory::ModelIdMeterThreePhase:
    case SunSpecModelFactory::ModelIdMeterThreePhaseDeltaConnect:
    case SunSpecModelFactory::ModelIdMeterSinglePhaseFloat:
    case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseFloat:
    case SunSpecModelFactory::ModelIdMeterThreePhaseWyeConnect:
    case SunSpecModelFactory::ModelIdDeltaConnectThreePhaseAbcMeter:
        return ModelFamilyMeter;
    case SunSpecModelFactory::ModelIdStorage:
        return ModelFamilyStorage;
    default:
        return ModelFamilyUnknown;
    }
}

bool isDuplicate(const QString &serialNumber, ModelFamily family, const QList<KnownThing> &knownThings)
{
    if (serialNumber.isEmpty())
        return false;

    foreach (const KnownThing &known, knownThings) {
        if (known.serialNumber != serialNumber)
            continue;

        if (family == ModelFamilyUnknown || known.family == ModelFamilyUnknown)
            return true;

        if (known.family == family)
            return true;
    }

    return false;
}

}
