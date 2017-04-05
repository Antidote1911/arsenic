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
