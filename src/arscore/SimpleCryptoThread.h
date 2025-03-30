#pragma once

#include <QObject>
#include <QThread>

#include "consts.h"
#include "libexport.h"

#ifdef SIMPLECRYPTOTHREAD_EXPORT
#define SIMPLECRYPTOTHREAD_API Q_DECL_EXPORT
#else
#define SIMPLECRYPTOTHREAD_API Q_DECL_IMPORT
#endif

class LIB_EXPORT Simple_Crypto_Thread : public QThread {
    Q_OBJECT

  public:
    explicit Simple_Crypto_Thread(QObject *parent = 0);
    void run();

    void setParam(bool direction,
                  QStringList const &filenames,
                  QString const &password,
                  QString const &algo,
                  quint32 const argonmem,
                  quint32 const argoniter,
                  bool const deletefile);

    void abort();

  signals:
    void updateProgress(const QString &path, quint32 percent);
    void statusMessage(const QString &message);
    void addEncrypted(const QString &inputFileName);
    void deletedAfterSuccess(const QString &inputFileName);

  private:
    quint32 encrypt(const QString &algo, const QString &src_path);
    quint32 decrypt(const QString &src_path);
    QStringList m_filenames;
    QString m_password;
    QString m_algo;
    quint32 m_argonmem;
    quint32 m_argoniter;
    bool m_direction;
    bool m_deletefile;
    bool m_aborted = false;

    const std::unique_ptr<consts> m_const;
};
