#ifndef ARGONHASH_H
#define ARGONHASH_H

#include "botan/botan_all.h"
#include <QString>


std::vector<uint8_t> pwdHash(QString passwordStr, Botan::secure_vector<uint8_t> salt, size_t outlen);



#endif // ARGONHASH_H
