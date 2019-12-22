#ifndef DECRYPT_H
#define DECRYPT_H

#include <QString>

QString decrypt(QString file, QString pass);

QString myDecryptFile(QString des_path, QString src_path, QString key, QString *decrypt_name = nullptr);


#endif // DECRYPT_H
