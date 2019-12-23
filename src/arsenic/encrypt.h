#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <QString>

QString encrypt(QString file, QString pass, QString add);
QString myEncryptFile(QString src_path, QString encrypt_path, QString key, QString name);

#endif // ENCRYPT_H
