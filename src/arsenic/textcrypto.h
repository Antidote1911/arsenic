#ifndef TEXTCRYPTO_H
#define TEXTCRYPTO_H

#include <QObject>

class textCrypto : public QObject
{
    Q_OBJECT
public:
    explicit textCrypto(QObject *parent = nullptr);
    int encryptString(QString plaintext, QString password);
    int decryptString(QString ciphertext, QString password);

    QString getResult();

private:
    QString m_result;

signals:
};

#endif // TEXTCRYPTO_H
