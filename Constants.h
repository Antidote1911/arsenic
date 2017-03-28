#ifndef CONSTANTS
#define CONSTANTS

#include <QString>

namespace BBp  // for Arsenic Preferences
{

// Global app infos
const QString APP_SHORT_NAME      = "Arsenic";
const QString APP_VERSION         = "1.0.0";
const QString APP_NAME            = "Arsenic_v"+APP_VERSION;
const QString APP_ORGANIZATION    = "Team1911";
const QString APP_URL             = "https://github.com/Antidote1911";

// Argon2 parameters
const uint32_t T_COST             = 4;      // n-pass computation
const uint32_t M_COST             = 1<<16;  // 64 MiB memory usage
const uint32_t PARALLELISM        = 2;      // number of threads and lanes


// Icon size
const int32_t FIELD_SIZE        = 24;
const int32_t ICON_SIZE         = 16;

// Default game preferences
const QString DEFAULT_CRYPTO_ALGO    = "ChaChaPoly1305";
const QString DEFAULT_HASH_ALGO      = "Whirlpool";
const QString DEFAULT_ENCODING       = "Base64_Encoder";

const int32_t MSG_TIMEOUT       = 5000;

const double MIN_DENSITY        = 0.05;
const double MAX_DENSITY        = 0.95;

} // namespace BBp

#endif // CONSTANTS
