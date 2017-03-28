#ifndef ARGONHASH_H
#define ARGONHASH_H

#include "crypto/botan/botan_all.h"
#include <QString>


std::string pwdHash(QString passwordStr, Botan::secure_vector<uint8_t> salt, size_t outlen);



#endif // ARGONHASH_H
