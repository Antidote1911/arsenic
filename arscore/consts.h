#pragma once

#include <QObject>
#include <QVersionNumber>

#include "botan_all.h"
#include "libexport.h"

#ifdef CONSTS_EXPORT
#define CRYPTOTHREAD_API Q_DECL_EXPORT
#else
#define CONSTS_API Q_DECL_IMPORT
#endif

class LIB_EXPORT consts : public QObject {
    Q_OBJECT
  public:
    explicit consts(QObject *parent = nullptr);

    static inline int const EXIT_CODE_REBOOT = -123456789;
    static inline QVersionNumber const APP_VERSION{5, 2};
    static inline QString const APP_SHORT_NAME         = "Arsenic";
    static inline QString const APP_LONG_NAME          = "Arsenic " + APP_VERSION.toString();
    static inline QString const APP_DESCRIPTION        = "Arsenic - Strong encryption";
    static inline QString const APP_URL                = "https://github.com/Antidote1911";
    static inline quint32 const MAGIC_NUMBER           = 0x41525345;
    static inline quint32 const CRYPTOBOX_VERSION_CODE = 0x2EC4993A;
    static inline quint32 const VERSION_CODE_LEN       = 4;

    // Default constants for Arsenic preferences
    static inline QString const DEFAULT_EXTENSION = ".arsn";

    static inline QString const BOTAN_VERSION = QString::fromStdString(Botan::version_string());

    static inline QString const DEFAULT_LIST_PATH = "session.qtlist";
    static inline int const MIN_PASS_LENGTH       = 8;

    // Default constants for Crypto engine
    static inline quint32 const MACBYTES       = 16;
    static inline quint32 const IN_BUFFER_SIZE = 65536;
    static inline quint32 const CIPHER_KEY_LEN = 32;
    static inline quint32 const CIPHER_SIV_KEY_LEN = 64;
    static inline quint32 const CIPHER_IV_LEN  = 24;

    // Argon2 constants
    static inline quint32 const ARGON_SALT_LEN       = 16;
    static inline quint32 const MEMLIMIT_INTERACTIVE = 65536;  // 64mb
    static inline quint32 const MEMLIMIT_MODERATE    = 524288; // 512mb
    static inline quint32 const MEMLIMIT_SENSITIVE   = 976563; //  1000mb

    static inline quint32 const ITERATION_INTERACTIVE = 1;
    static inline quint32 const ITERATION_MODERATE    = 2;
    static inline quint32 const ITERATION_SENSITIVE   = 3;

    static inline quint32 const PARALLELISM_INTERACTIVE = 1; // For now, Argon2 in Botan don't use Parallelism > 1

  signals:
};
