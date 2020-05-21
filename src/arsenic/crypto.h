#ifndef CRYPTO_H
#define CRYPTO_H

#include "botan_all.h"
#include <QObject>
#include <QThread>

class Crypto_Thread : public QThread {
    Q_OBJECT

public:
    explicit Crypto_Thread(QObject* parent = 0);
    void run();
    bool aborted = false;

    void setParam(bool direction,
        QStringList filenames,
        const QString password,
        quint32 argonmem,
        quint32 argoniter,
        bool deletefile);

    void encrypt(const QString filename);
    void decrypt(const QString filename);
    QString outfileresult;
    bool m_deletefile;

signals:
    void updateProgress(const QString& path, quint32 percent);
    void statusMessage(QString message);
    void addEncrypted(QString inputFileName);
    void addDecrypted(QString inputFileName);
    void sourceDeletedAfterSuccess(QString inputFileName);

private:
    bool mstop;
    Botan::SecureVector<char> convertStringToSecureVector(QString password);
    Botan::SecureVector<quint8> calculateHash(Botan::SecureVector<char> pass_buffer,
        Botan::SecureVector<quint8> salt_buffer,
        quint32 memlimit,
        quint32 iterations);

    QString removeExtension(const QString& fileName, const QString& extension);

    QString uniqueFileName(const QString& fileName);

    QStringList m_filenames;
    QString m_password;
    QString m_encoding;
    quint32 m_argonmem;
    quint32 m_argoniter;
    bool m_direction;
};

#endif // CRYPTO_H
