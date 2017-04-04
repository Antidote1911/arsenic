#ifndef CRYPTO_THREAD_H
#define CRYPTO_THREAD_H

#include <QThread>

#if defined(Q_OS_LINUX)
 #if defined(__clang__)
   #include "botan/clang/botan_all.h"
 #elif defined(__GNUC__) || defined(__GNUG__)
   #include "botan/gcc/botan_all.h"
#endif
#endif

#if defined(Q_OS_WIN64)
  #include "botan/msvc_x64/botan_all.h"
#endif


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
