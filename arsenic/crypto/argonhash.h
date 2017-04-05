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

#ifndef ARGONHASH_H
#define ARGONHASH_H
#include <string>
#include <vector>

#if defined(__clang__)
#include "botan/clang/botan_all.h"
#elif defined(__GNUC__) || defined(__GNUG__)
#include "botan/gcc/botan_all.h"
#endif


std::vector<uint8_t> pwdHashRaw(uint32_t time_cost, uint32_t memory_cost, uint32_t parallelism,
                                std::string passwordStr,
                                std::string salt,
                                size_t outlen);


std::vector<char> pwdHashEncoded(uint32_t time_cost, uint32_t memory_cost, uint32_t parallelism,
                                 std::string passwordStr,
                                 std::string salt,
                                 size_t outlen);

#endif // ARGONHASH_H
