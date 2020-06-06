#ifndef TEXTCRYPTO_H
#define TEXTCRYPTO_H

#include <QObject>

class textCrypto : public QObject
{
    Q_OBJECT
public:
    explicit textCrypto(QObject *parent = nullptr);

    void start(QString password, int dirrection);
    void finish(QString &text);

private:
    void encryptString(QString &plaintext, QString password);
    void decryptString(QString &ciphertext, QString password);

    QString m_password;
    int m_dirrection;

signals:
};

#endif // TEXTCRYPTO_H
