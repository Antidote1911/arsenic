#ifndef CRYPTO_THREAD_H
#define CRYPTO_THREAD_H

#include <QThread>
#include "botan/botan_all.h"


class Crypto_Thread : public QThread
{
    Q_OBJECT
public:
    explicit Crypto_Thread(QObject *parent = 0);
    void run();
    bool aborted=false;
    bool cancel=false;
    void setParam(QStringList filenames, QString password, QString algo, bool encryptBool, QString encoding, bool deletefile, QString version);
    QString outfileresult;

signals:
    void PercentProgressChanged(const QString& path, qint64 percent, QString source);
    void statusMessage(QString, QString, QString);

private:
    QStringList m_filenames;
    QString m_password;
    QString m_algo;
    QString m_encoding;
    bool m_encryptBool;
    bool m_deletefile;
    QString m_version;

    void decryptFile(const QString& filename, const QString& password, QString version);

    void encryptFile(QString& filename, QString& password, QString& algo, QString& encoding, QString version);

    void executeCipher(const QString& inputFileName,
                       Botan::Pipe& pipe,
                       std::ifstream& in,
                       std::ofstream& out,
                       QString outfilename);

    QString removeExtension(const QString& fileName, const QString& extension);

    QString uniqueFileName(const QString& fileName);

    void abort();


public slots:


};

#endif // CRYPTO_THREAD_H
