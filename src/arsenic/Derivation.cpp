#include "Derivation.h"
#include "botan_all.h"
#include "constants.h"

using namespace Botan;
using namespace ARs;

Derivation::Derivation(QObject *parent) : QObject(parent)
{
}

void Derivation::setPassword(QString password)
{
    m_password = password;
}

void Derivation::setSalt(Botan::SecureVector<quint8> salt)
{
    m_salt = salt;
}

void Derivation::setArgonParam(quint32 memlimit, quint32 iterations)
{
    m_memlimit = memlimit;
    m_iterations = iterations;
    calculateMastersKeys();
}

void Derivation::calculateMastersKeys()
{
    const auto m_pass_buffer { convertStringToVectorChar(m_password) };
    auto pwdhash_fam { PasswordHashFamily::create("Argon2id") };
    SecureVector<quint8> key_buffer(CIPHER_KEY_LEN * 3);

    // mem,ops,threads
    auto default_pwhash { pwdhash_fam->from_params(m_memlimit, m_iterations, PARALLELISM_INTERACTIVE) };

    default_pwhash->derive_key(key_buffer.data(),
                               key_buffer.size(),
                               m_pass_buffer.data(),
                               m_pass_buffer.size(),
                               m_salt.data(),
                               m_salt.size());

    const auto* mk { key_buffer.begin().base() };
    const SymmetricKey ChaCha20_key(mk, CIPHER_KEY_LEN);
    const SymmetricKey AES_key(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey Serpent_key(&mk[CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    m_key1 = ChaCha20_key;
    m_key2 = AES_key;
    m_key3 = Serpent_key;
}

Botan::SymmetricKey Derivation::getkey1()
{
    return(m_key1);
}
Botan::SymmetricKey Derivation::getkey2()
{
    return(m_key2);
}
Botan::SymmetricKey Derivation::getkey3()
{
    return(m_key3);
}

SecureVector<char> Derivation::convertStringToVectorChar(QString qstring)
{
    const auto pass { qstring.toStdString() };
    SecureVector<char> passv(pass.begin(), pass.end());
    return(passv);
}
