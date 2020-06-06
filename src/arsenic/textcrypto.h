#ifndef TEXTCRYPTO_H
#define TEXTCRYPTO_H

#include <QObject>

class textCrypto : public QObject
{
    Q_OBJECT
public:
    explicit textCrypto(QObject *parent = nullptr);

    void start(QString password, int dirrection);
    int finish(QString &text);

private:
    int encryptString(QString &plaintext, QString password);
    int decryptString(QString &ciphertext, QString password);

    QString m_password;
    int m_dirrection;

signals:
};

#endif // TEXTCRYPTO_H
