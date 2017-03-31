#include "botan/botan_all.h"
#include "argonhash.h"
#include "../preferences/preferences.h"
#include <iostream>

#include "argon2/argon2.h"


using namespace std;

vector<uint8_t> pwdHash(string user_password, Botan::secure_vector<uint8_t> user_salt, size_t outlen) {

    vector<uint8_t> output(outlen);

    auto ret = argon2_hash(ARs::T_COST,
                           ARs::M_COST,
                           ARs::PARALLELISM,
                           user_password.data(),
                           user_password.size(),
                           user_salt.data(),
                           user_salt.size(),
                           output.data(), outlen, NULL, 0, Argon2_id, ARGON2_VERSION_NUMBER);

    if (ret != ARGON2_OK)
    {
        cerr << "ARGON2 ERROR : " << argon2_error_message(ret) << endl;
    }

    //Botan::hex_encode(output,output.size())

    return output;

}
