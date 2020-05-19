// clang-format off
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "botan_all.h"
#include <QDebug>
#include <QTranslator>
#include <QtEndian>

namespace ARs // for Arsenic Preferences
{

// Global app infos
const QString APP_VERSION           { "2.5" };
const QString APP_SHORT_NAME        { "Arsenic" };
const QString APP_LONG_NAME         { "Arsenic " + APP_VERSION };
const QString APP_DESCRIPTION       { "Strong encryption" };
const QString APP_URL               { "https://github.com/Antidote1911" };
const quint32 MAGIC_NUMBER          { 0x41525345 };

// Default constants for Arsenic preferences
const QString DEFAULT_EXTENSION     { ".arsenic" };
const QString DEFAULT_LANGUAGE      { "en" };
const QString DEFAULT_LASTDIRECTORY { "" };
const int DEFAULT_INDEX_TAB         { 0 };
const bool DEFAULT_SHOW_PSW         { false };
const bool DEFAULT_DELETE_FINSHED   { false };

const bool DEFAULT_ADD_ENCRYPTED    { true };
const bool DEFAULT_ADD_DECRYPTED    { true };
const int DEFAULT_ARGON_ITR_LIMIT   { 0 };
const int DEFAULT_ARGON_MEM_LIMIT   { 0 };

const bool DEFAULT_DARK_THEME       { false };
const bool DEFAULT_SHOW_TOOLBAR     { true };
const bool DEFAULT_CLEAR_CLIPBOARD  { true };
const int DEFAULT_CLIPBOARD_TIMEOUT { 10 };
const QString BOTAN_VERSION         { QString::fromStdString(Botan::version_string()) };

const QString DEFAULT_LIST_PATH     { "session.qtlist" };
const int MIN_PASS_LENGTH           { 8 };

// Default constants for Crypto engine
const int MACBYTES                  { 16 };
const int IN_BUFFER_SIZE            { 65536 };
const size_t CIPHER_KEY_LEN         { 32 };
const size_t CIPHER_IV_LEN          { 24 };

// Argon2 constants
const int ARGON_SALT_LEN            { 16 };
const int MEMLIMIT_INTERACTIVE      { 65536 }; // 64mb
const int MEMLIMIT_MODERATE         { 524288 }; // 512mb
const int MEMLIMIT_SENSITIVE        { 976563 }; //  1000mb

const int ITERATION_INTERACTIVE     { 1 };
const int ITERATION_MODERATE        { 2 };
const int ITERATION_SENSITIVE       { 3 };

const int PARALLELISM_INTERACTIVE   { 1 }; // For now, Argon2 in Botan don't use Parallelism > 1

} // namespace ARs

#endif // CONSTANTS_H

// clang-format on
