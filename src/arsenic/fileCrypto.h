#ifndef FILECRYPTO_H
#define FILECRYPTO_H

#include "botan_all.h"
#include <QObject>
#include <QThread>

class Crypto_Thread : public QThread
{
    Q_OBJECT

public:
    explicit Crypto_Thread(QObject *parent = 0);
    void run();

    void setParam(bool direction, QStringList filenames, const QString password, quint32 argonmem, quint32 argoniter, bool deletefile);
    int result;

    qint32 encrypt(const QString filename);
    qint32 decrypt(const QString filename);
    void abort();

signals:
    void updateProgress(const QString &path, quint32 percent);
    void statusMessage(QString message);
    void addEncrypted(QString inputFileName);
    void addDecrypted(QString inputFileName);
    void sourceDeletedAfterSuccess(QString inputFileName);

private:
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
