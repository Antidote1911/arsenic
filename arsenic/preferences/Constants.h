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

#ifndef CONSTANTS
#define CONSTANTS

#include <QString>

namespace ARs  // for Arsenic Preferences
{

// Global app infos
const QString APP_SHORT_NAME         = "Arsenic";
const QString APP_VERSION            = "0.9.9";
const QString APP_NAME               = "Arsenic_v"+APP_VERSION;
const QString APP_ORGANIZATION       = "Team1911";
const QString APP_URL                = "https://github.com/Antidote1911";

// Default app preferences
const QString DEFAULT_CRYPTO_ALGO    = "ChaCha20Poly1305";
const QString DEFAULT_HASH_ALGO      = "Whirlpool";
const QString DEFAULT_ENCODING       = "Base64_Encoder";

// Argon2 parameters
const uint32_t T_COST                = 4;      // n-pass computation
const uint32_t M_COST                = 1<<16;  // 64 MiB memory usage
const uint32_t PARALLELISM           = 2;      // number of threads and lanes

} // namespace ARs

#endif // CONSTANTS
