#include "tripleencryption.h"
#include "constants.h"

using namespace Botan;
using namespace ARs;

TripleEncryption::TripleEncryption(int mode, QObject *parent) : QObject(parent)
{
    if (mode == 0) {
        m_direction = ENCRYPTION;
    }
    else {
        m_direction = DECRYPTION;
    }

    m_engineChacha = AEAD_Mode::create("ChaCha20Poly1305", m_direction);
    m_engineAes = AEAD_Mode::create("AES-256/EAX", m_direction);
    m_engineSerpent = AEAD_Mode::create("Serpent/GCM", m_direction);
}

void TripleEncryption::setSalt(Botan::OctetString salt)
{
    m_salt = salt;
}

void TripleEncryption::generateSalt()
{
    AutoSeeded_RNG rng;
    auto argonSalt { rng.random_vec(ARGON_SALT_LEN) };
    m_salt = argonSalt;
}

void TripleEncryption::derivePassword(QString password, quint32 memlimit, quint32 iterations)
{
    const auto pass { password.toStdString() };
    SecureVector<char> pass_buffer(pass.begin(), pass.end());

    auto pwdhash_fam { PasswordHashFamily::create("Argon2id") };
    SecureVector<quint8> key_buffer(CIPHER_KEY_LEN * 3);

    // mem,ops,threads
    auto default_pwhash { pwdhash_fam->from_params(memlimit, iterations, PARALLELISM_INTERACTIVE) };

    default_pwhash->derive_key(key_buffer.data(),
                               key_buffer.size(),
                               pass_buffer.data(),
                               pass_buffer.size(),
                               m_salt.bits_of().data(),
                               m_salt.size());

    const auto* mk { key_buffer.begin().base() };
    const SymmetricKey ChaCha20_key(mk, CIPHER_KEY_LEN);
    const SymmetricKey AES_key(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey Serpent_key(&mk[CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    m_chachaKey = ChaCha20_key;
    m_aesKey = AES_key;
    m_serpentKey = Serpent_key;
    m_masterKey = key_buffer;

    m_engineChacha->set_key(ChaCha20_key);
    m_engineAes->set_key(AES_key);
    m_engineSerpent->set_key(Serpent_key);
}


void TripleEncryption::setTripleKey(SymmetricKey masterKey)
{
    // split the triple nonce

    const auto* n { masterKey.begin() };
    const SymmetricKey chachaKey(n, CIPHER_KEY_LEN);
    const SymmetricKey aesKey(&n[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey serpentKey(&n[CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    m_engineChacha->set_key(chachaKey);
    m_engineAes->set_key(aesKey);
    m_engineSerpent->set_key(serpentKey);
}


void TripleEncryption::setTripleNonce(InitializationVector nonce)
{
    // split the triple nonce
    const auto* n { nonce.begin() };
    const InitializationVector iv1(n, CIPHER_IV_LEN);
    const InitializationVector iv2(&n[CIPHER_IV_LEN], CIPHER_IV_LEN);
    const InitializationVector iv3(&n[CIPHER_IV_LEN + CIPHER_IV_LEN], CIPHER_IV_LEN);

    m_nonceChaCha20 = iv1.bits_of();
    m_nonceAes = iv2.bits_of();
    m_nonceSerpent = iv3.bits_of();
}

void TripleEncryption::setAdd(QString add)
{
    const auto strAdd { add.toStdString() };
    SecureVector<quint8> vecAdd(strAdd.begin(), strAdd.end());

    m_engineChacha->set_ad(vecAdd);
    m_engineAes->set_ad(vecAdd);
    m_engineSerpent->set_ad(vecAdd);
}

void TripleEncryption::incrementNonce()
{
    auto tmp1 = m_nonceChaCha20.bits_of();
    Botan::Sodium::sodium_increment(tmp1.data(), CIPHER_IV_LEN);
    m_nonceChaCha20 = tmp1;

    auto tmp2 = m_nonceAes.bits_of();
    Botan::Sodium::sodium_increment(tmp2.data(), CIPHER_IV_LEN);
    m_nonceAes = tmp2;

    auto tmp3 = m_nonceSerpent.bits_of();
    Botan::Sodium::sodium_increment(tmp3.data(), CIPHER_IV_LEN);
    m_nonceSerpent = tmp3;
}

SecureVector<quint8> TripleEncryption::finish(SecureVector<quint8> buffer)
{
    if (m_direction == ENCRYPTION) {
        incrementNonce();
        auto test = m_nonceChaCha20.bits_of();
        m_engineChacha->start(m_nonceChaCha20.bits_of());
        m_engineChacha->finish(buffer);

        m_engineAes->start(m_nonceAes.bits_of());
        m_engineAes->finish(buffer);

        m_engineSerpent->start(m_nonceSerpent.bits_of());
        m_engineSerpent->finish(buffer);
    }
    else {
        incrementNonce();
        m_engineSerpent->start(m_nonceSerpent.bits_of());
        m_engineSerpent->finish(buffer);

        m_engineAes->start(m_nonceAes.bits_of());
        m_engineAes->finish(buffer);

        m_engineChacha->start(m_nonceChaCha20.bits_of());
        m_engineChacha->finish(buffer);
    }

    outBuffer = buffer;
    return(buffer);
}

void TripleEncryption::generateTripleNonce()
{
    AutoSeeded_RNG rng;
    auto nonceChaCha20 { rng.random_vec(CIPHER_IV_LEN) };
    auto nonceAes { rng.random_vec(CIPHER_IV_LEN) };
    auto nonceSerpent { rng.random_vec(CIPHER_IV_LEN) };

    m_nonceChaCha20 = nonceChaCha20;
    m_nonceAes = nonceAes;
    m_nonceSerpent = nonceSerpent;

    nonceChaCha20.insert(nonceChaCha20.end(), nonceAes.begin(), nonceAes.end());
    nonceChaCha20.insert(nonceChaCha20.end(), nonceSerpent.begin(), nonceSerpent.end());
    m_tripleNonce = nonceChaCha20;
}

Botan::InitializationVector TripleEncryption::getTripleNonce()
{
    return(m_tripleNonce);
}

Botan::OctetString TripleEncryption::getSalt()
{
    return(m_salt);
}
