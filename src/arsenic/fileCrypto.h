#ifndef FILECRYPTO_H
#define FILECRYPTO_H

#include <QObject>
#include <QThread>

#include "botan_all.h"

class Crypto_Thread : public QThread {
    Q_OBJECT

  public:
    explicit Crypto_Thread(QObject *parent = 0);
    void run();

    void setParam(bool direction,
                  QStringList const &filenames,
                  QString const &password,
                  quint32 const argonmem,
                  quint32 const argoniter,
                  bool const deletefile);

    void abort();

  signals:
    void updateProgress(const QString &path, quint32 percent);
    void statusMessage(const QString &message);
    void addEncrypted(const QString &inputFileName);
    void addDecrypted(const QString &inputFileName);
    void deletedAfterSuccess(const QString &inputFileName);

  private:
    quint32 encrypt(const QString &src_path);
    quint32 decrypt(const QString &src_path);
    QString uniqueFileName(const QString &fileName);
    QStringList m_filenames;
    QString m_password;
    quint32 m_argonmem;
    quint32 m_argoniter;
    bool m_direction;
    bool m_deletefile;
    bool m_aborted = false;
};

#endif // FILECRYPTO_H
