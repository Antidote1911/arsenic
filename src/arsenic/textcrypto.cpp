#include "textcrypto.h"
#include "messages.h"
#include "botan_all.h"
#include "constants.h"
#include <QString>
#include <QTextStream>
#include "tripleencryption.h"

using namespace ARs;
using namespace Botan;
using namespace std;

textCrypto::textCrypto(QObject *parent) : QObject(parent)
{
}
int textCrypto::encryptString(QString plaintext, QString password)
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

    const auto CRYPTOBOX_VERSION_CODE { 0x2EC4993A };
    const auto VERSION_CODE_LEN { 4 };

    const auto clear { plaintext.toStdString() };
    // Copy input data to a buffer
    SecureVector<quint8> pt(clear.data(), clear.data() + clear.length());

    // Now we can do the triple encryption
    // Randomize the 16 bytes salt and the three 24 bytes nonces
    AutoSeeded_RNG rng;
    const auto argonSalt { rng.random_vec(ARGON_SALT_LEN) };
    const auto tripleNonce { rng.random_vec(CIPHER_IV_LEN * 3) };

    TripleEncryption encrypt(0);
    encrypt.setSalt(argonSalt);
    encrypt.derivePassword(password, MEMLIMIT_INTERACTIVE, ITERATION_INTERACTIVE);
    encrypt.setTripleNonce(tripleNonce);
    encrypt.finish(pt);

    SecureVector<quint8> final (VERSION_CODE_LEN);
    for (size_t i = 0; i != VERSION_CODE_LEN; ++i)
    {
        final[i] = get_byte(i, CRYPTOBOX_VERSION_CODE);
    }
    final.insert(final.end(), argonSalt.begin(), argonSalt.end());
    final.insert(final.end(), tripleNonce.begin(), tripleNonce.end());
    final.insert(final.end(), pt.begin(), pt.end());

    m_result = QString::fromStdString(PEM_Code::encode(final, "ARSENIC CRYPTOBOX MESSAGE"));
    return(CRYPT_SUCCESS);
}

int textCrypto::decryptString(QString cipher, QString password)
{
    const auto CRYPTOBOX_HEADER_LEN { VERSION_CODE_LEN + ARGON_SALT_LEN + CIPHER_IV_LEN * 3 };
    const auto cipherStr { cipher.toStdString() };

    DataSource_Memory input_src(cipherStr);
    SecureVector<quint8> ciphertext;
    try {
        ciphertext = PEM_Code::decode_check_label(input_src, "ARSENIC CRYPTOBOX MESSAGE");
    }
    catch (const Botan::Decoding_Error& e)
    {
        return(BAD_CRYPTOBOX_PEM_HEADER);
    }

    if (ciphertext.size() < CRYPTOBOX_HEADER_LEN) {
        return(INVALID_CRYPTOBOX_IMPUT);
    }

    for (auto i { 0 }; i != VERSION_CODE_LEN; ++i)
    {
        if (ciphertext[i] != get_byte(i, CRYPTOBOX_VERSION_CODE)) {
            return(BAD_CRYPTOBOX_VERSION);
        }
    }
    const auto* tmp { ciphertext.begin().base() };
    const OctetString version(tmp, VERSION_CODE_LEN);
    const OctetString salt(&tmp[VERSION_CODE_LEN], ARGON_SALT_LEN);
    const InitializationVector tripleNonce(&tmp[VERSION_CODE_LEN + ARGON_SALT_LEN], CIPHER_IV_LEN * 3);

    ciphertext.erase(ciphertext.begin(), ciphertext.begin() + CRYPTOBOX_HEADER_LEN);

    // Now we can do the triple decryption
    TripleEncryption decrypt(1);
    decrypt.setSalt(salt);
    decrypt.derivePassword(password, MEMLIMIT_INTERACTIVE, ITERATION_INTERACTIVE);
    decrypt.setTripleNonce(tripleNonce.bits_of());
    SecureVector<quint8> outbuffer;
    try {
        outbuffer = decrypt.finish(ciphertext);
    }
    catch (const Botan::Invalid_Authentication_Tag& e)
    {
        return(INVALID_TAG);
    }
    catch (const Botan::Integrity_Failure&)
    {
        return(INTEGRITY_FAILURE);
    }
    catch (const Botan::Decoding_Error&)
    {
        return(INTEGRITY_FAILURE);
    }

    const string out(outbuffer.begin(), outbuffer.end()); //std::string out(reinterpret_cast<const char*>(outbuffer.data()), outbuffer.size());
    m_result = QString::fromStdString(out);
    return(DECRYPT_SUCCESS);
}

QString textCrypto::getResult()
{
    return(m_result);
}
