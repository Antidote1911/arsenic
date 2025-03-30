#pragma once

#include "botan_all.h"
#include "consts.h"

#include <QObject>
#include <memory>

class SimpleCryptoEngine : public QObject {
    Q_OBJECT
  public:
    explicit SimpleCryptoEngine(QString algo, bool direction = true, QObject *parent = nullptr);

    void setSalt(const Botan::SecureVector<quint8> &salt);
    void derivePassword(const QString &password, quint32 memlimit, quint32 iterations);
    void setNonce(const Botan::SecureVector<quint8> &nonce);
    void finish(Botan::SecureVector<quint8> &buffer);

  private:
    void incrementNonce();
    Botan::Cipher_Dir m_direction;
    Botan::SecureVector<quint8> m_nonce;

    std::unique_ptr<Botan::AEAD_Mode> m_engine;

    Botan::SecureVector<quint8> m_salt;
    QString m_algo;

    const std::unique_ptr<consts> m_const;

  signals:
};
