#ifndef CONSTS_H
#define CONSTS_H

#include <QObject>
#include <QVersionNumber>

#include "botan_all.h"
#include "zlib.h"

class consts : public QObject {
    Q_OBJECT
  public:
    explicit consts(QObject *parent = nullptr);

    static inline QString const S = "Forty-Two";
    static inline QVersionNumber const APP_VERSION{3, 5, 2};
    static inline QString const APP_SHORT_NAME         = "Arsenic";
    static inline QString const APP_LONG_NAME          = "Arsenic " + APP_VERSION.toString();
    static inline QString const APP_DESCRIPTION        = "Strong encryption";
    static inline QString const APP_URL                = "https://github.com/Antidote1911";
    static inline quint32 const MAGIC_NUMBER           = 0x41525345;
    static inline quint32 const CRYPTOBOX_VERSION_CODE = 0x2EC4993A;
    static inline quint32 const VERSION_CODE_LEN       = 4;

    // Default constants for Arsenic preferences
    static inline QString const DEFAULT_EXTENSION     = ".arsenic";
    static inline QString const DEFAULT_LANGUAGE      = "en";
    static inline QString const DEFAULT_LASTDIRECTORY = "";
    static inline quint32 const DEFAULT_INDEX_TAB     = 0;
    static inline bool const DEFAULT_SHOW_PSW         = false;
    static inline bool const DEFAULT_DELETE_FINSHED   = false;

    static inline bool const DEFAULT_ADD_ENCRYPTED      = true;
    static inline bool const DEFAULT_ADD_DECRYPTED      = true;
    static inline quint32 const DEFAULT_ARGON_ITR_LIMIT = 0;
    static inline quint32 const DEFAULT_ARGON_MEM_LIMIT = 0;

    static inline bool const DEFAULT_DARK_THEME           = false;
    static inline bool const DEFAULT_SHOW_TOOLBAR         = true;
    static inline bool const DEFAULT_CLEAR_CLIPBOARD      = true;
    static inline quint32 const DEFAULT_CLIPBOARD_TIMEOUT = 10;
    static inline QString const BOTAN_VERSION             = QString::fromStdString(Botan::version_string());
    static inline QString const ZLIB_Version              = ZLIB_VERSION;

    static inline QString const DEFAULT_LIST_PATH = "session.qtlist";
    static inline int const MIN_PASS_LENGTH       = 8;

    // Default constants for Crypto engine
    static inline quint32 const MACBYTES       = 16;
    static inline quint32 const IN_BUFFER_SIZE = 65536;
    static inline quint32 const CIPHER_KEY_LEN = 32;
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

#endif // CONSTS_H
