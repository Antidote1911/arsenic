#ifndef CONSTANTS_H
#define CONSTANTS_H


#include "quazip.h"
#include "zlib.h"
#include <QtEndian>
#include <QDebug>
#include <QTranslator>
#include "botan_all.h"

namespace ARs  // for Arsenic Preferences
{

// Global app infos
const QString APP_VERSION            {"1.0"};
const QString APP_SHORT_NAME         {"Arsenic"};
const QString APP_LONG_NAME          {"Arsenic "+APP_VERSION};
const QString APP_ORGANIZATION       {"Team1911"};
const QString APP_DESCRIPTION        {QObject::tr("Strongs files or directory encryption")};
const QString APP_URL                {"https://github.com/Antidote1911"};
const quint32 MAGIC_NUMBER           {0x41525345};

// Default constants for Arsenic preferences
const QString DEFAULT_EXTENSION      {"arsenic"};
const QString DEFAULT_USER_NAME      {""};
const QString DEFAULT_LANGUAGE       {"en"};
const int DEFAULT_ARGON_ITR_LIMIT    {0};
const int DEFAULT_ARGON_MEM_LIMIT    {0};
const bool DEFAULT_SHOW_PSW          {false};
const bool DEFAULT_DARK_THEME        {false};
const QString BOTAN_VERSION          {QString::fromStdString(Botan::version_string())};
const QString ZLIB_Version           {ZLIB_VERSION};

const QString DEFAULT_LIST_PATH      {"session.qtlist"};
const int MIN_PASS_LENGTH            {8};
const int START_ENCRYPT              {0};
const int START_DECRYPT              {1};
const int MAX_ENCRYPT_NAME_LENGTH    {256};
const int EXTRA_COLUMN_SPACE         {5};
const int EMPTY_COLUMN_SPACE         {100};

// Default constants for Crypto engine
const QString DEFAULT_CRYPTO_ALGO    {"ChaCha20Poly1305"};
const int MACBYTES                   {16};
const int IN_BUFFER_SIZE             {65536};
const int KEYBYTES                   {32};
const int NONCEBYTES                 {24};
const int SALTBYTES                  {16};
const int BUFFER_SIZE_WITHOUT_MAC    {(IN_BUFFER_SIZE - MACBYTES)};

// Argon2 constants
const int MEMLIMIT_INTERACTIVE       {65536};  // 64mb
const int MEMLIMIT_MODERATE          {524288}; // 512mb
const int MEMLIMIT_SENSITIVE         {976563}; //  1000mb

const int ITERATION_INTERACTIVE      {1};
const int ITERATION_MODERATE         {2};
const int ITERATION_SENSITIVE        {3};

const int PARALLELISM_INTERACTIVE    {1}; // For now, Argon2 in Botan don't use Parallelism > 1



} // namespace ARs


#endif // CONSTANTS_H
