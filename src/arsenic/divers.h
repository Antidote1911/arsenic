#ifndef DIVERS_H
#define DIVERS_H

#include <QString>
#include <QTextStream>

#include "botan_all.h"

static QTextStream cin(stdin);
static QTextStream cout(stdout);
void clearDir(QString dir_path);
qint64 dirSize(QString dirPath);
QString getFileSize(qint64 size);

Botan::SecureVector<char> convertStringToSecureVector(QString qstring);
QString errorCodeToString(int error_code);

QString encryptString(QString plaintext, QString password);
QString decryptString(QString ciphertext, QString password);

#endif // DIVERS_H
