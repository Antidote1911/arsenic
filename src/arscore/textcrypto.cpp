#include "textcrypto.h"
#include "botan_all.h"
#include "messages.h"
#include "triplecryptoengine.h"
#include <QString>
#include <QTextStream>
#include <stdexcept>

using namespace Botan;
using namespace std;

textCrypto::textCrypto(QObject *parent)
    : QObject(parent)
{
}
quint32 textCrypto::encryptString(QString &plaintext, QString const &password)
{
    /* Version code is First 24 bits of SHA-256("Arsenic Cryptobox")
     * Output format is:
     * version    (4 bytes)
     * salt       (16 bytes) for Argon2
     * nonce1     (24 bytes) for ChaCha20Poly1305
     * nonce2     (24 bytes) for AES-256/GCM
     * nonce3     (24 bytes) for Serpent/GCM
     * ciphertext
     */

    const auto CRYPTOBOX_VERSION_CODE = 0x2EC4993A;
    const auto VERSION_CODE_LEN       = 4;

    const auto clear = plaintext.toStdString();
    // Copy input data to a buffer
    SecureVector<quint8> pt(clear.data(), clear.data() + clear.length());

    // Now we can do the triple encryption
    // Randomize the 16 bytes salt and the three 24 bytes nonces
    AutoSeeded_RNG rng;
    const auto argonSalt   = rng.random_vec(m_const->ARGON_SALT_LEN);
    const auto tripleNonce = rng.random_vec(m_const->CIPHER_IV_LEN * 3);

    TripleCryptoEngine encrypt(true);
    encrypt.setSalt(argonSalt);
    encrypt.derivePassword(password, m_const->MEMLIMIT_INTERACTIVE, m_const->ITERATION_INTERACTIVE);
    encrypt.setNonce(tripleNonce);
    encrypt.finish(pt);

    SecureVector<quint8> final(VERSION_CODE_LEN);
    for (size_t i = 0; i != VERSION_CODE_LEN; ++i) {
        final[i] = get_byte_var(i, CRYPTOBOX_VERSION_CODE);
    }
    final.insert(final.end(), argonSalt.begin(), argonSalt.end());
    final.insert(final.end(), tripleNonce.begin(), tripleNonce.end());
    final.insert(final.end(), pt.begin(), pt.end());

    plaintext = (QString::fromStdString(PEM_Code::encode(final, "ARSENIC CRYPTOBOX MESSAGE")));
    return (CRYPT_SUCCESS);
}

quint32 textCrypto::decryptString(QString &cipher, QString const &password)
{
    const auto CRYPTOBOX_HEADER_LEN = m_const->VERSION_CODE_LEN + m_const->ARGON_SALT_LEN + m_const->CIPHER_IV_LEN * 3;
    const auto cipherStr            = cipher.toStdString();

    DataSource_Memory input_src(cipherStr);
    SecureVector<quint8> ciphertext;
    try {
        ciphertext = PEM_Code::decode_check_label(input_src, "ARSENIC CRYPTOBOX MESSAGE");
    }
    catch (Botan::Exception const &e) {
        return (BAD_CRYPTOBOX_PEM_HEADER);
    }
    if (ciphertext.size() < CRYPTOBOX_HEADER_LEN) {
        return (INVALID_CRYPTOBOX_IMPUT);
    }
    for (quint32 i = 0; i != m_const->VERSION_CODE_LEN; ++i)
        if (ciphertext[i] != get_byte_var(i, m_const->CRYPTOBOX_VERSION_CODE)) {
            return (BAD_CRYPTOBOX_VERSION);
        }
    const auto *tmp{ &ciphertext[0]};
    const OctetString version(tmp, m_const->VERSION_CODE_LEN);
    const OctetString salt(&tmp[m_const->VERSION_CODE_LEN], m_const->ARGON_SALT_LEN);
    const InitializationVector tripleNonce(&tmp[m_const->VERSION_CODE_LEN + m_const->ARGON_SALT_LEN], m_const->CIPHER_IV_LEN * 3);

    ciphertext.erase(ciphertext.begin(), ciphertext.begin() + CRYPTOBOX_HEADER_LEN);

    // Now we can do the triple decryption
    TripleCryptoEngine decrypt(false);
    decrypt.setSalt(salt.bits_of());
    decrypt.derivePassword(password, m_const->MEMLIMIT_INTERACTIVE, m_const->ITERATION_INTERACTIVE);
    decrypt.setNonce(tripleNonce.bits_of());
    try {
        decrypt.finish(ciphertext);
    }
    catch (const Botan::Exception &) {
        return (DECRYPT_FAIL);
    }

    const string out(ciphertext.begin(), ciphertext.end()); // std::string out(reinterpret_cast<const char*>(outbuffer.data()), outbuffer.size());
    cipher = QString::fromStdString(out);
    return (DECRYPT_SUCCESS);
}

void textCrypto::start(const QString &password, bool dirrection)
{
    m_password   = password;
    m_dirrection = dirrection;
}

quint32 textCrypto::finish(QString &text)
{
    quint32 result;
    if (m_dirrection) {
        result = encryptString(text, m_password);
    }
    else {
        result = decryptString(text, m_password);
    }
    return (result);
}
