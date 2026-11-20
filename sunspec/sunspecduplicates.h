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

#ifndef SUNSPECDUPLICATES_H
#define SUNSPECDUPLICATES_H

#include <QList>
#include <QString>

/* The serial number guard of the automatic thing creation, kept free of the
 * integration framework so it can be exercised without a running nymea.
 *
 * Some devices announce the same model a second time on other registers, for a
 * while, and the plugin must not create a second thing for it. Comparing the
 * model ID and the start register is not enough to catch that, hence the
 * comparison on the serial number of the common block.
 *
 * But a serial number belongs to a common block, not to a model: every model
 * behind one common block carries it. A storage model 124 sitting behind the
 * common block of its inverter has the inverter's serial number. Comparing on
 * the serial number alone therefore refuses the storage as a "duplicate" of
 * its own inverter as soon as the inverter thing exists - that is, whenever the
 * storage model shows up after the first setup: a battery added later, or a
 * restart after it appeared. The first setup only escapes because no child
 * thing is registered yet while all of them are announced.
 *
 * So a duplicate is the same serial number AND the same kind of model. */
namespace SunSpecDuplicates {

enum ModelFamily {
    ModelFamilyUnknown,
    ModelFamilyInverter,
    ModelFamilyMeter,
    ModelFamilyStorage
};

/* The kind of device a model describes. The single, split and three phase
 * variants of a family are the same kind on purpose: a device reporting a
 * single phase meter on some register and a three phase meter a few minutes
 * later is the very case the guard exists for. */
ModelFamily modelFamily(quint16 modelId);

struct KnownThing {
    QString serialNumber;
    ModelFamily family;
};

/* Whether a thing for a model of `family`, behind a common block carrying
 * `serialNumber`, would duplicate one of `knownThings`.
 *
 * An empty serial number proves nothing and never matches. When either side's
 * family is unknown, the serial number alone decides, as it did before the
 * family was taken into account: nothing is let through that could not be told
 * apart. */
bool isDuplicate(const QString &serialNumber, ModelFamily family, const QList<KnownThing> &knownThings);

}

#endif // SUNSPECDUPLICATES_H
