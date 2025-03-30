#pragma once

#include "botan_all.h"
#include "consts.h"

#include <QObject>
#include <memory>

class TripleCryptoEngine : public QObject {
    Q_OBJECT
  public:
    explicit TripleCryptoEngine(bool direction = true, QObject *parent = nullptr);

    void setSalt(const Botan::SecureVector<quint8> &salt);
    void derivePassword(const QString &password, quint32 memlimit, quint32 iterations);
    void setNonce(const Botan::SecureVector<quint8> &nonce);
    void finish(Botan::SecureVector<quint8> &buffer);

  private:
    void incrementNonce();
    Botan::Cipher_Dir m_direction;
    Botan::SecureVector<quint8> m_nonceChaCha20;
    Botan::SecureVector<quint8> m_nonceAes;
    Botan::SecureVector<quint8> m_nonceSerpent;

    std::unique_ptr<Botan::AEAD_Mode> m_engineChacha;
    std::unique_ptr<Botan::AEAD_Mode> m_engineAes;
    std::unique_ptr<Botan::AEAD_Mode> m_engineSerpent;

    Botan::SecureVector<quint8> m_salt;

    const std::unique_ptr<consts> m_const;

  signals:
};
