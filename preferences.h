#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "Constants.h"
#include <QString>
#include <stdint.h>

namespace BBp
{

struct Preferences
{
    Preferences() : algoCrypto(DEFAULT_CRYPTO_ALGO),
        algoHash(DEFAULT_HASH_ALGO),
        encoding(DEFAULT_ENCODING)
    {}

    QString algoCrypto;
    QString algoHash;
    QString encoding;
    QString lastDirectory;
    bool deleteOriginals;
    QByteArray restoreGeometry;
    QByteArray restoreState;
    int cipherDirection;
};

} // namespace BBp

#endif // PREFERENCES_H
