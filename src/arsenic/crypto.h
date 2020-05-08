#ifndef CRYPTO_H
#define CRYPTO_H

#include <QObject>
#include <QThread>
#include "botan_all.h"


class Crypto_Thread : public QThread
{
    Q_OBJECT
public:
    explicit Crypto_Thread(QObject *parent = 0);
    void run();
    bool aborted=false;

    void setParam(bool direction,
                  QStringList filenames,
                  const QString password,
                  const QString algo,
                  int argonmem,
                  int argoniter,
                  bool deletefile);

    void encrypt(const QString filename);
    void decrypt(const QString filename);
    QString outfileresult;
    bool m_deletefile;

signals:
    void updateProgress(const QString& path, qint64 percent);
    void statusMessage(QString message);

private:
    bool mstop;
    Botan::SecureVector<char> convertStringToSecureVector(QString password);
    Botan::SecureVector<quint8> calculateHash(Botan::SecureVector<char> pass_buffer,
                                              Botan::SecureVector<quint8> salt_buffer,
                                              size_t memlimit,
                                              size_t iterations);

    QString removeExtension(const QString& fileName, const QString& extension);

    QString uniqueFileName(const QString& fileName);


    QStringList m_filenames;
    QString m_password;
    QString m_algo;
    QString m_encoding;
    int m_argonmem;
    int m_argoniter;
    bool m_direction;

};

#endif // CRYPTO_THREAD_H
