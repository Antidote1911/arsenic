#ifndef TRIPLEENCRYPTION_H
#define TRIPLEENCRYPTION_H

#include "botan_all.h"
#include <QObject>


class TripleEncryption : public QObject
{
    Q_OBJECT
public:
    explicit TripleEncryption(int mode = 0, QObject *parent = nullptr);

    void generateSalt();
    void setSalt(Botan::OctetString salt);
    Botan::OctetString getSalt();
    void derivePassword(QString pass, quint32 memlimit, quint32 iterations);

    void generateTripleNonce();
    void setTripleNonce(Botan::InitializationVector nonce);
    Botan::InitializationVector getTripleNonce();

    void setTripleKey(Botan::SymmetricKey masterKey);
    void setAdd(QString add);

    Botan::SecureVector<quint8> finish(Botan::SecureVector<quint8> buffer);

private:
    void incrementNonce();
    Botan::SecureVector<quint8> m_incrementedChachaNonce;
    Botan::SecureVector<quint8> m_incrementedAesNonce;
    Botan::SecureVector<quint8> m_incrementedSerpentNonce;
    Botan::Cipher_Dir m_direction;

    Botan::InitializationVector m_tripleNonce;
    Botan::InitializationVector m_nonceChaCha20;
    Botan::InitializationVector m_nonceAes;
    Botan::InitializationVector m_nonceSerpent;

    std::unique_ptr<Botan::AEAD_Mode> m_engineChacha;
    std::unique_ptr<Botan::AEAD_Mode> m_engineAes;
    std::unique_ptr<Botan::AEAD_Mode> m_engineSerpent;

    Botan::SecureVector<quint8> outBuffer;

    QString m_password;
    Botan::SymmetricKey m_chachaKey;
    Botan::SymmetricKey m_aesKey;
    Botan::SymmetricKey m_serpentKey;
    Botan::SymmetricKey m_masterKey;
    Botan::OctetString m_salt;

signals:
};

#endif // TRIPLEENCRYPTION_H
