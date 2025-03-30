#include "simplecryptoengine.h"
#include <cassert>
#include "botan_all.h"

using namespace Botan;

SimpleCryptoEngine::SimpleCryptoEngine(QString algo, bool direction, QObject *parent)
    : QObject(parent)
{

    if (direction) {
        m_direction = Botan::Cipher_Dir::Encryption;
    }
    else {
        m_direction = Botan::Cipher_Dir::Decryption;
    }

    m_engine  = AEAD_Mode::create(algo.toStdString(), m_direction);
    m_algo = algo;

}

void SimpleCryptoEngine::setSalt(const Botan::SecureVector<quint8> &salt)
{
    assert(salt.size() == m_const->ARGON_SALT_LEN && "Salt must be 16 bytes.");
    m_salt = salt;
}

void SimpleCryptoEngine::derivePassword(const QString &password, quint32 memlimit, quint32 iterations)
{
    const auto pass{password.toStdString()};
    SecureVector<char> pass_buffer(pass.begin(), pass.end());

    auto pwdhash_fam{PasswordHashFamily::create("Argon2id")};

    int keylen;
    if (m_algo == "Serpent/GCM"||m_algo=="Serpent/EAX"||m_algo=="AES-256/GCM"||m_algo=="AES-256/EAX"||m_algo== "ChaCha20Poly1305" ){
        // need a 32 bits key
        keylen = m_const->CIPHER_KEY_LEN;
    } else{
        // this is a SIV, need a 64 bits key
        keylen=m_const->CIPHER_SIV_KEY_LEN;
    }

    SecureVector<quint8> key_buffer(keylen);

    // mem,ops,threads
    const auto default_pwhash{pwdhash_fam->from_params(memlimit, iterations, m_const->PARALLELISM_INTERACTIVE)};

    default_pwhash->derive_key(key_buffer.data(),
                               key_buffer.size(),
                               pass_buffer.data(),
                               pass_buffer.size(),
                               m_salt.data(),
                               m_salt.size());

    const SymmetricKey Engine_key(key_buffer.data(), keylen);
    m_engine->set_key(Engine_key);
}

void SimpleCryptoEngine::setNonce(const SecureVector<quint8> &nonce)
{
    assert(nonce.size() == m_const->CIPHER_IV_LEN && "Triple nonce must be 24 bytes.");
    // split the triple nonce
    const auto *n{ &nonce[0]};
    const InitializationVector iv(n, m_const->CIPHER_IV_LEN);

    m_nonce = iv.bits_of();
}

void SimpleCryptoEngine::incrementNonce()
{

    Sodium::sodium_increment(m_nonce.data(), m_const->CIPHER_IV_LEN);
}

void SimpleCryptoEngine::finish(SecureVector<quint8> &buffer)
{

    if (m_direction == Botan::Cipher_Dir::Encryption) {
        incrementNonce();
        m_engine->start(m_nonce);
        m_engine->finish(buffer);
    }
    else {
        incrementNonce();
        m_engine->start(m_nonce);
        m_engine->finish(buffer);
    }
}
