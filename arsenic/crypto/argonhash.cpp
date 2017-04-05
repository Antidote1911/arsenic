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

#include "argonhash.h"
#include "../preferences/preferences.h"
#include <iostream>

#include "argon2/argon2.h"


using namespace std;

vector<uint8_t> pwdHashRaw(uint32_t time_cost, uint32_t memory_cost, uint32_t parallelism,
                           string user_password,
                           string user_salt,
                           size_t outlen) {

    vector<uint8_t> output(outlen);

    auto ret = argon2_hash(time_cost,
                           memory_cost,
                           parallelism,
                           user_password.data(),
                           user_password.size(),
                           user_salt.data(),
                           user_salt.size(),
                           output.data(), outlen, NULL, 0, Argon2_i, ARGON2_VERSION_NUMBER);

    if (ret != ARGON2_OK)
    {
        cerr << "ARGON2 ERROR : " << argon2_error_message(ret) << endl;
    }

    //Botan::hex_encode(output,output.size())

    return output;

}

vector<char> pwdHashEncoded(uint32_t time_cost, uint32_t memory_cost, uint32_t parallelism,
                            string user_password,
                            string user_salt,
                            size_t outlen) {

    vector<uint8_t> outputRaw(outlen);
    size_t encodedlen = argon2_encodedlen(time_cost, memory_cost, parallelism, (uint32_t)user_salt.size(), outlen, Argon2_i);
    cout<<encodedlen<<endl;
    vector<char> outputEncoded(encodedlen);

    auto ret = argon2_hash(time_cost, memory_cost, parallelism,
                           user_password.data(), user_password.size(),
                           user_salt.data(), user_salt.size(),
                           outputRaw.data(), outputRaw.size(),
                           outputEncoded.data(), outputEncoded.size(),
                           Argon2_i, ARGON2_VERSION_NUMBER);

    if (ret != ARGON2_OK)
    {
        cerr << "ARGON2 ERROR : " << argon2_error_message(ret) << endl;
    }

    //Botan::hex_encode(output,output.size())

    return outputEncoded;
}
