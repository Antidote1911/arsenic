#include "crypto/botan/botan_all.h"
#include "crypto/argonhash.h"
#include "crypto/argon2/argon2.h"
#include "preferences.h"
#include <iostream>


using namespace std;


string pwdHash(QString user_password, Botan::secure_vector<uint8_t> user_salt, size_t outlen) {

    vector<uint8_t> output(outlen);
    size_t passwordLength = static_cast<size_t>(user_password.size());


    auto ret = argon2_hash(ARs::T_COST,
                           ARs::M_COST,
                           ARs::PARALLELISM,
                           user_password.data(),
                           passwordLength,
                           user_salt.data(),
                           user_salt.size(),
                           output.data(), outlen, NULL, 0, Argon2_id, ARGON2_VERSION_NUMBER);

    if (ret != ARGON2_OK)
    {
        cerr << "ARGON2 ERROR : " << argon2_error_message(ret) << endl;
    }

    return Botan::hex_encode(output,output.size());

}
