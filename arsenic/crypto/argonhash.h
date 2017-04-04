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
