#ifndef DIVERS_H
#define DIVERS_H

#include <QString>
#include <QTextStream>

#include "botan_all.h"

static QTextStream cin(stdin);
static QTextStream cout(stdout);
void clearDir(QString dir_path);
qint64 dirSize(QString dirPath);
QString formatSize(qint64 size);

Botan::SecureVector<quint8> calculateHash(Botan::SecureVector<char> pass_buffer,
                                          Botan::SecureVector<quint8> salt_buffer,
                                          size_t memlimit,
                                          size_t iterations);

Botan::SecureVector<char> convertStringToSecureVector(QString password);


#endif // DIVERS_H