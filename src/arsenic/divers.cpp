#include "divers.h"
#include "constants.h"
#include <QDir>
#include <QString>
#include <QTextStream>
#include <cmath>
#include "Derivation.h"

#include <QtGlobal>
#if defined(Q_OS_UNIX)
    #include "botan_all.h"
#endif

#if defined(Q_OS_WIN)
    #include "botan_all.h"
#endif

using namespace ARs;
using namespace Botan;
using namespace std;

QString getFileSize(qint64 size)
{
    static const double KiB = pow(2, 10);
    static const double MiB = pow(2, 20);
    static const double GiB = pow(2, 30);
    static const double TiB = pow(2, 40);
    static const double PiB = pow(2, 50);

    // convert to appropriate units based on the size of the item
    if (size >= 0) {
        static const int precision = 0;

        if (size < KiB) {
            return(QString::number(size, 'f', precision) + " B");
        }
        else if (size < MiB) {
            return(QString::number(size / KiB, 'f', precision) + " KiB");
        }
        else if (size < GiB) {
            return(QString::number(size / MiB, 'f', precision) + " MiB");
        }
        else if (size < TiB) {
            return(QString::number(size / GiB, 'f', precision) + " GiB");
        }
        else if (size < PiB) {
            return(QString::number(size / TiB, 'f', precision) + " TiB");
        }
        else {
            return(QString::number(size / PiB, 'f', precision) + " PiB");
        }
    }
    else {
        return("");
    }
}


void clearDir(QString dir_path)
{
    QDir qd(dir_path);

    if (qd.exists()) {
        QList<QFileInfo> item_list = qd.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden);

        for (auto it = item_list.begin(); it != item_list.end(); ++it)
        {
            if (it->isDir()) {
                QDir(it->absoluteFilePath()).removeRecursively();
            }
            else if (it->isFile()) {
                QDir().remove(it->absoluteFilePath());
            }
        }
    }
}


qint64 dirSize(QString dirPath)
{
    qint64 size = 0;
    QDir dir(dirPath);
    //calculate total size of current directories' files
    QDir::Filters fileFilters = QDir::Files | QDir::System | QDir::Hidden;
    for (QString filePath : dir.entryList(fileFilters))
    {
        QFileInfo fi(dir, filePath);
        size += fi.size();
    }
    //add size of child directories recursively
    QDir::Filters dirFilters = QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden;
    for (QString childDirPath : dir.entryList(dirFilters))
    {
        size += dirSize(dirPath + QDir::separator() + childDirPath);
    }
    return(size);
}


Botan::SecureVector<char> convertStringToSecureVector(QString password)
{
    Botan::SecureVector<char> pass_buffer(password.size());
    memset(pass_buffer.data(), 0, password.size());
    memcpy(pass_buffer.data(), password.toUtf8().constData(), password.toUtf8().size());
    return(pass_buffer);
}


QString encryptString(QString plaintext, QString password)
{
    /*
     * Output format is:
     * version    (4 bytes)
     * salt       (10 bytes) for Argon2
     * nonce1     (24 bytes) for ChaCha20Poly1305
     * nonce2     (24 bytes) for AES-256/GCM
     * nonce3     (24 bytes) for Serpent/GCM
     * ciphertext
     */

    // Copy input data to a buffer
    string clear = plaintext.toStdString();
    SecureVector<uint8_t> pt(clear.data(), clear.data() + clear.length());

    /*
     * Version code is First 24 bits of SHA-256("Arsenic Cryptobox"), followed by 8 0 bits
     * for later use as flags, etc if needed
     */
    const uint32_t CRYPTOBOX_VERSION_CODE = 0x2EC4993A;
    const size_t VERSION_CODE_LEN = 4;

    const size_t CRYPTOBOX_HEADER_LEN = VERSION_CODE_LEN + ARGON_SALT_LEN + CIPHER_IV_LEN * 3;

    string add_data = APP_URL.toStdString();
    vector<uint8_t> add(add_data.data(), add_data.data() + add_data.length());

    std::unique_ptr<RandomNumberGenerator> rng(new AutoSeeded_RNG);

    SecureVector<uint8_t> out_buf(CRYPTOBOX_HEADER_LEN + pt.size());
    for (size_t i = 0; i != VERSION_CODE_LEN; ++i)
    {
        out_buf[i] = get_byte(i, CRYPTOBOX_VERSION_CODE);
    }

    rng->randomize(&out_buf[VERSION_CODE_LEN], ARGON_SALT_LEN);

    const Botan::OctetString argon_salt(&out_buf[VERSION_CODE_LEN], ARGON_SALT_LEN);

    // Generate 3*24 bytes random nonce and copy result in out_buf
    SecureVector<uint8_t> master_iv_buffer(rng->random_vec(CIPHER_IV_LEN * 3));
    copy_mem(&out_buf[VERSION_CODE_LEN + ARGON_SALT_LEN], master_iv_buffer.data(), master_iv_buffer.size());

    if (pt.size() > 0) {
        copy_mem(&out_buf[CRYPTOBOX_HEADER_LEN], pt.data(), pt.size());
    }

    // Generate the 3*32 bytes master key by Argon2
    Derivation deriv;
    deriv.setPassword(password);
    deriv.setSalt(argon_salt.bits_of());
    deriv.setArgonParam(MEMLIMIT_INTERACTIVE, ITERATION_INTERACTIVE);

    // Split master_iv in tree parts.
    const uint8_t *iv = master_iv_buffer.begin().base();
    const InitializationVector iv_1(iv, CIPHER_IV_LEN);
    const InitializationVector iv_2(&iv[CIPHER_IV_LEN], CIPHER_IV_LEN);
    const InitializationVector iv_3(&iv[CIPHER_IV_LEN + CIPHER_IV_LEN], CIPHER_IV_LEN);

    // Now we can do the triple encryption
    std::unique_ptr<AEAD_Mode> enc = AEAD_Mode::create("ChaCha20Poly1305", ENCRYPTION);
    enc->set_key(deriv.getkey1());
    enc->set_ad(add);
    enc->start(iv_1.bits_of());
    enc->finish(out_buf, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<AEAD_Mode> enc2 = AEAD_Mode::create("AES-256/EAX", ENCRYPTION);
    enc2->set_key(deriv.getkey2());
    enc2->set_ad(add);
    enc2->start(iv_2.bits_of());
    enc2->finish(out_buf, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<AEAD_Mode> enc3 = AEAD_Mode::create("Serpent/GCM", ENCRYPTION);
    enc3->set_key(deriv.getkey3());
    enc3->set_ad(add);
    enc3->start(iv_3.bits_of());
    enc3->finish(out_buf, CRYPTOBOX_HEADER_LEN);

    return(QString::fromStdString(PEM_Code::encode(out_buf, "ARSENIC CRYPTOBOX MESSAGE")));
}


QString decryptString(QString cipher, QString password)
{
    const string cipherStr = cipher.toStdString();

    string add_data = APP_URL.toStdString();
    SecureVector<uint8_t> add(add_data.data(), add_data.data() + add_data.length());

    const uint32_t CRYPTOBOX_VERSION_CODE = 0x2EC4993A;
    const size_t VERSION_CODE_LEN = 4;

    const size_t CRYPTOBOX_HEADER_LEN = VERSION_CODE_LEN + ARGON_SALT_LEN + CIPHER_IV_LEN * 3;

    DataSource_Memory input_src(cipherStr);
    SecureVector<uint8_t> ciphertext = PEM_Code::decode_check_label(input_src, "ARSENIC CRYPTOBOX MESSAGE");

    if (ciphertext.size() < CRYPTOBOX_HEADER_LEN) {
        throw Decoding_Error("Invalid CryptoBox input");
    }

    for (size_t i = 0; i != VERSION_CODE_LEN; ++i)
    {
        if (ciphertext[i] != get_byte(i, CRYPTOBOX_VERSION_CODE)) {
            throw Decoding_Error("Bad CryptoBox version");
        }
    }

    //const uint8_t *argon_salt = &ciphertext[VERSION_CODE_LEN];
    const OctetString argon_salt(&ciphertext[VERSION_CODE_LEN], ARGON_SALT_LEN);
    const uint8_t *iv = &ciphertext[VERSION_CODE_LEN + ARGON_SALT_LEN];

    // mem,ops,threads
    Derivation deriv;
    deriv.setPassword(password);
    deriv.setSalt(argon_salt.bits_of());
    deriv.setArgonParam(MEMLIMIT_INTERACTIVE, ITERATION_INTERACTIVE);

    // Split iv_buffer in tree parts.
    const InitializationVector iv_1(iv, CIPHER_IV_LEN);
    const InitializationVector iv_2(&iv[CIPHER_IV_LEN], CIPHER_IV_LEN);
    const InitializationVector iv_3(&iv[CIPHER_IV_LEN + CIPHER_IV_LEN], CIPHER_IV_LEN);

    std::unique_ptr<AEAD_Mode> dec = AEAD_Mode::create("Serpent/GCM", DECRYPTION);
    dec->set_key(deriv.getkey3());
    dec->set_ad(add);
    dec->start(iv_3.bits_of());
    dec->finish(ciphertext, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<AEAD_Mode> dec2 = AEAD_Mode::create("AES-256/EAX", DECRYPTION);
    dec2->set_key(deriv.getkey2());
    dec2->set_ad(add);
    dec2->start(iv_2.bits_of());
    dec2->finish(ciphertext, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<Botan::AEAD_Mode> dec3 = Botan::AEAD_Mode::create("ChaCha20Poly1305", Botan::DECRYPTION);
    dec3->set_key(deriv.getkey1());
    dec3->set_ad(add);
    dec3->start(iv_1.bits_of());
    dec3->finish(ciphertext, CRYPTOBOX_HEADER_LEN);

    ciphertext.erase(ciphertext.begin(), ciphertext.begin() + CRYPTOBOX_HEADER_LEN);

    const string out(ciphertext.begin(), ciphertext.end()); //std::string out(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    return(QString::fromStdString(out));
}
