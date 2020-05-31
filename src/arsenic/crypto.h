#ifndef CRYPTO_H
#define CRYPTO_H


#include "botan_all.h"
#include <QObject>
#include <QThread>


class Crypto_Thread : public QThread {
    Q_OBJECT

public:
    explicit Crypto_Thread(QObject *parent = 0);
    void run();

    bool aborted = false;

    void setParam(bool direction,
                  QStringList filenames,
                  const QString password,
                  quint32 argonmem,
                  quint32 argoniter,
                  bool deletefile);


    qint32 encrypt(const QString filename);
    qint32 decrypt(const QString filename);
    int result;

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
    Botan::SecureVector<char> convertStringToVectorChar(QString qstring);
    Botan::SecureVector<char> convertStringToVectorChar(std::string string);

    Botan::SecureVector<quint8> convertStringToVectorquint8(QString qstring);
    Botan::SecureVector<quint8> convertStringToVectorquint8(std::string string);
    Botan::SecureVector<quint8> convertIntToVectorquint8(qint64 num);

    Botan::SecureVector<quint8> calculateHash(Botan::SecureVector<char> pass_buffer,
                                              Botan::SecureVector<quint8> salt_buffer,
                                              quint32 memlimit,
                                              quint32 iterations);

    QString removeExtension(const QString& fileName,
                            const QString& extension);

    QString uniqueFileName(const QString& fileName);

    QStringList m_filenames;
    QString m_password;
    QString m_encoding;
    quint32 m_argonmem;
    quint32 m_argoniter;
    bool m_direction;
    QString errorCodeToString(int error_code);
};

#endif // CRYPTO_H
