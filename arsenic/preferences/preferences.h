/*
 * This file is part of Arsenic.
 *
 * Copyright (C) 2017 Corraire Fabrice <antidote1911@gmail.com>
 *
 * Arsenic is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * Arsenic is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Arsenic. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "Constants.h"
#include <QString>
#include <stdint.h>

namespace ARs
{

struct Preferences
{
    Preferences() : algoCrypto(DEFAULT_CRYPTO_ALGO),
        algoHash(DEFAULT_HASH_ALGO),
        encoding(DEFAULT_ENCODING)
    {}

    QString algoCrypto;
    QString algoHash;
    QString encoding;
    QString lastDirectory;
    bool deleteOriginals;
    QByteArray restoreGeometry;
    QByteArray restoreState;
    int cipherDirection;
};

} // namespace ARs

#endif // PREFERENCES_H
