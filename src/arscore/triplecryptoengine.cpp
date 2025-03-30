#include "triplecryptoengine.h"
#include <cassert>

#include "botan_all.h"

using namespace Botan;

TripleCryptoEngine::TripleCryptoEngine(bool direction, QObject *parent)
    : QObject(parent)
{

    if (direction) {
        m_direction = Botan::Cipher_Dir::Encryption;
    }
    else {
        m_direction = Botan::Cipher_Dir::Decryption;
    }

    m_engineChacha  = AEAD_Mode::create("ChaCha20Poly1305", m_direction);
    m_engineAes     = AEAD_Mode::create("AES-256/EAX", m_direction);
    m_engineSerpent = AEAD_Mode::create("Serpent/GCM", m_direction);
}

void TripleCryptoEngine::setSalt(const SecureVector<quint8> &salt)
{
    assert(salt.size() == m_const->ARGON_SALT_LEN && "Salt must be 16 bytes.");
    m_salt = salt;
}

void TripleCryptoEngine::derivePassword(const QString &password, quint32 memlimit, quint32 iterations)
{
    const auto pass{password.toStdString()};
    SecureVector<char> pass_buffer(pass.begin(), pass.end());

    auto pwdhash_fam{PasswordHashFamily::create("Argon2id")};

    SecureVector<quint8> key_buffer(m_const->CIPHER_KEY_LEN * 3);

    // mem,ops,threads
    const auto default_pwhash{pwdhash_fam->from_params(memlimit, iterations, m_const->PARALLELISM_INTERACTIVE)};

    default_pwhash->derive_key(key_buffer.data(),
                               key_buffer.size(),
                               pass_buffer.data(),
                               pass_buffer.size(),
                               m_salt.data(),
                               m_salt.size());

    const auto *mk{ &key_buffer[0]};
    const SymmetricKey ChaCha20_key(mk, m_const->CIPHER_KEY_LEN);
    const SymmetricKey AES_key(&mk[m_const->CIPHER_KEY_LEN], m_const->CIPHER_KEY_LEN);
    const SymmetricKey Serpent_key(&mk[m_const->CIPHER_KEY_LEN + m_const->CIPHER_KEY_LEN], m_const->CIPHER_KEY_LEN);

    m_engineChacha->set_key(ChaCha20_key);
    m_engineAes->set_key(AES_key);
    m_engineSerpent->set_key(Serpent_key);
}

void TripleCryptoEngine::setNonce(const SecureVector<quint8> &nonce)
{
    assert(nonce.size() == m_const->CIPHER_IV_LEN * 3 && "Triple nonce must be 24*3 bytes.");
    // split the triple nonce
    const auto *n{&nonce[0]};
    const InitializationVector iv1(n, m_const->CIPHER_IV_LEN);
    const InitializationVector iv2(&n[m_const->CIPHER_IV_LEN], m_const->CIPHER_IV_LEN);
    const InitializationVector iv3(&n[m_const->CIPHER_IV_LEN + m_const->CIPHER_IV_LEN], m_const->CIPHER_IV_LEN);

    m_nonceChaCha20 = iv1.bits_of();
    m_nonceAes      = iv2.bits_of();
    m_nonceSerpent  = iv3.bits_of();
}

void TripleCryptoEngine::incrementNonce()
{

    Sodium::sodium_increment(m_nonceChaCha20.data(), m_const->CIPHER_IV_LEN);
    Sodium::sodium_increment(m_nonceAes.data(), m_const->CIPHER_IV_LEN);
    Sodium::sodium_increment(m_nonceSerpent.data(), m_const->CIPHER_IV_LEN);
}

void TripleCryptoEngine::finish(SecureVector<quint8> &buffer)
{

    if (m_direction == Botan::Cipher_Dir::Encryption) {
        incrementNonce();
        m_engineChacha->start(m_nonceChaCha20);
        m_engineChacha->finish(buffer);

        m_engineAes->start(m_nonceAes);
        m_engineAes->finish(buffer);

        m_engineSerpent->start(m_nonceSerpent);
        m_engineSerpent->finish(buffer);
    }
    else {
        incrementNonce();
        m_engineSerpent->start(m_nonceSerpent);
        m_engineSerpent->finish(buffer);

        m_engineAes->start(m_nonceAes);
        m_engineAes->finish(buffer);

        m_engineChacha->start(m_nonceChaCha20);
        m_engineChacha->finish(buffer);
    }
}
