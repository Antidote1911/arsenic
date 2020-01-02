#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "constants.h"
#include <QString>
#include <stdint.h>

namespace ARs
{

struct Preferences
{
    Preferences() : showPassword(DEFAULT_SHOW_PSW)
      ,darkTheme(DEFAULT_DARK_THEME)
      ,extension(DEFAULT_EXTENSION)
      ,argonMemory(DEFAULT_ARGON_MEM_LIMIT)
      ,argonItr(DEFAULT_ARGON_ITR_LIMIT)
      ,cryptoAlgo(DEFAULT_CRYPTO_ALGO)
      ,userName(DEFAULT_USER_NAME)

    {}

    bool showPassword;
    bool darkTheme;
    QString extension;
    int argonMemory;
    int argonItr;
    QString cryptoAlgo;
    QString userName;

};

} // namespace ARs

#endif // PREFERENCES_H
