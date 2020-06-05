#ifndef TRIPLEENCRYPTION_H
#define TRIPLEENCRYPTION_H

#include "botan_all.h"
#include "constants.h"
#include <QObject>


class TripleEncryption : public QObject
{
    Q_OBJECT
public:
    explicit TripleEncryption(int mode = 0, QObject *parent = nullptr);

    void setSalt(Botan::OctetString salt);
    void derivePassword(QString pass, quint32 memlimit, quint32 iterations);
    void setTripleNonce(Botan::SecureVector<quint8> nonce);
    void setTripleKey(Botan::SymmetricKey masterKey);
    Botan::SecureVector<quint8> finish(Botan::SecureVector<quint8> &buffer);

private:
    void incrementNonce();
    Botan::Cipher_Dir m_direction;
    Botan::SecureVector<quint8> m_tripleNonce;
    Botan::SecureVector<quint8> m_nonceChaCha20;
    Botan::SecureVector<quint8> m_nonceAes;
    Botan::SecureVector<quint8> m_nonceSerpent;

    std::unique_ptr<Botan::AEAD_Mode> m_engineChacha;
    std::unique_ptr<Botan::AEAD_Mode> m_engineAes;
    std::unique_ptr<Botan::AEAD_Mode> m_engineSerpent;

    Botan::SecureVector<quint8> m_outBuffer;

    Botan::SymmetricKey m_chachaKey;
    Botan::SymmetricKey m_aesKey;
    Botan::SymmetricKey m_serpentKey;
    Botan::SymmetricKey m_masterKey;
    Botan::OctetString m_salt;

signals:
};

#endif // TRIPLEENCRYPTION_H
