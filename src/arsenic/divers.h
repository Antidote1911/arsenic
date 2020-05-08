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

Botan::SecureVector<quint8> calculateHash(Botan::SecureVector<char> pass_buffer,
                                          Botan::SecureVector<quint8> salt_buffer,
                                          size_t memlimit,
                                          size_t iterations);

Botan::SecureVector<char> convertStringToSecureVector(QString password);
QString errorCodeToString(int error_code);

QString encryptString(QString plaintext,QString password, QString userName);
QString decryptString(QString ciphertext,QString password, QString userName);

#endif // DIVERS_H
