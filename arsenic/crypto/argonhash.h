#ifndef ARGONHASH_H
#define ARGONHASH_H

#include "botan/botan_all.h"


std::vector<uint8_t> pwdHashRaw(uint32_t time_cost, uint32_t memory_cost, uint32_t parallelism,
                                std::string passwordStr,
                                std::string salt,
                                size_t outlen);


std::vector<char> pwdHashEncoded(uint32_t time_cost, uint32_t memory_cost, uint32_t parallelism,
                                 std::string passwordStr,
                                 std::string salt,
                                 size_t outlen);

#endif // ARGONHASH_H
