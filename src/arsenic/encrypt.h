#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <QString>

QString encrypt(QString file, QString pass);
QString myEncryptFile(QString src_path, QString encrypt_path, QString key);

#endif // ENCRYPT_H
