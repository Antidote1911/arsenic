#ifndef TRIPLEENCRYPTION_H
#define TRIPLEENCRYPTION_H

#include "botan_all.h"
#include "consts.h"

#include <QObject>
#include <memory>

class TripleEncryption : public QObject {
    Q_OBJECT
  public:
    explicit TripleEncryption(bool mode = true, QObject *parent = nullptr);

    void setSalt(const Botan::OctetString &salt);
    void derivePassword(const QString &password, quint32 memlimit, quint32 iterations);
    void setTripleNonce(const Botan::SecureVector<quint8> &nonce);
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

    Botan::OctetString m_salt;

    const std::unique_ptr<consts> m_const;

  signals:
};

#endif // TRIPLEENCRYPTION_H
