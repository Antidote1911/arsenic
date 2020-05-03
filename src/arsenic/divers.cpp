#include "divers.h"
#include <QString>
#include <QDir>
#include <QTextStream>
#include "botan_all.h"
#include "constants.h"
#include "messages.h"

using namespace ARs;
using namespace MessagesPublic;
using namespace Botan;
using namespace std;


void clearDir(QString dir_path)
{
    QDir qd(dir_path);

    if(qd.exists())
    {
        QList<QFileInfo> item_list = qd.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoSymLinks |
            QDir::NoDotAndDotDot | QDir::Hidden);

        for(auto it = item_list.begin(); it != item_list.end(); ++it)
        {
            if(it->isDir())
                QDir(it->absoluteFilePath()).removeRecursively();
            else if(it->isFile())
                QDir().remove(it->absoluteFilePath());
        }
    }
}


    qint64 dirSize(QString dirPath)
    {
    qint64 size = 0;
    QDir dir(dirPath);
    //calculate total size of current directories' files
    QDir::Filters fileFilters = QDir::Files|QDir::System|QDir::Hidden;
    for(QString filePath : dir.entryList(fileFilters)) {
        QFileInfo fi(dir, filePath);
        size+= fi.size();
    }
    //add size of child directories recursively
    QDir::Filters dirFilters = QDir::Dirs|QDir::NoDotAndDotDot|QDir::System|QDir::Hidden;
    for(QString childDirPath : dir.entryList(dirFilters))
        size+= dirSize(dirPath + QDir::separator() + childDirPath);
    return size;
}

QString formatSize(qint64 size) {
    QStringList units = {"Bytes", "KB", "MB", "GB", "TB", "PB"};
    int i;
    double outputSize = size;
    for(i=0; i<units.size()-1; i++) {
        if(outputSize<1024) break;
        outputSize= outputSize/1024;
    }
    return QString("%0 %1").arg(outputSize, 0, 'f', 2).arg(units[i]);
}

Botan::SecureVector<quint8> calculateHash(Botan::SecureVector<char> pass_buffer,
                                          Botan::SecureVector<quint8> salt_buffer,
                                          size_t memlimit,
                                          size_t iterations)
{
    auto pwdhash_fam = Botan::PasswordHashFamily::create("Argon2id");
    Botan::SecureVector<quint8> key_buffer(CIPHER_KEY_LEN*3);

    // mem,ops,threads
    auto default_pwhash = pwdhash_fam->from_params(memlimit,iterations,PARALLELISM_INTERACTIVE);

    default_pwhash->derive_key(key_buffer.data(), key_buffer.size(),
                               pass_buffer.data(), pass_buffer.size(),
                               salt_buffer.data(), salt_buffer.size());
    return key_buffer;
}

Botan::SecureVector<char> convertStringToSecureVector(QString password)
{
    Botan::SecureVector<char> pass_buffer(CIPHER_KEY_LEN);
    memset(pass_buffer.data(), 0, CIPHER_KEY_LEN);
    memcpy(pass_buffer.data(), password.toUtf8().constData(), std::min(password.toUtf8().size(),
        static_cast<int>(CIPHER_KEY_LEN)));
    return pass_buffer;
}
QString errorCodeToString(int error_code)
{
    QString ret_string;

    switch(error_code)
    {
        case SRC_NOT_FOUND:
            ret_string += QObject::tr("The encrypted file was not found!");
            break;

        case SRC_CANNOT_OPEN_READ:
            ret_string += QObject::tr("The file could not be opened for reading!");
            break;

        case SRC_HEADER_READ_ERROR:
            ret_string += QObject::tr("The encrypted file header could not be read!");
            break;

        case PASS_HASH_FAIL:
            ret_string += QObject::tr("The password could not be hashed!");
            break;

        case SRC_HEADER_DECRYPT_ERROR:
            ret_string += QObject::tr("The encrypted file header could not be decrypted! Password is most likely "
                "incorrect or the header has been modified.");
            break;

        case DES_FILE_EXISTS:
            ret_string += QObject::tr("The decrypted file already exists!");
            break;

        case DES_CANNOT_OPEN_WRITE:
            ret_string += QObject::tr("The file could not be opened for writing!");
            break;

        case SRC_BODY_READ_ERROR:
            ret_string += QObject::tr("There was an error reading the encrypted file's data!");
            break;

        case SRC_FILE_CORRUPT:
            ret_string += QObject::tr("The encrypted file's size is invalid!");
            break;

        case DATA_DECRYPT_ERROR:
            ret_string += QObject::tr("The encrypted file's data could not be decrypted! The data has most likely "
                "been modified.");
            break;

        case DES_BODY_WRITE_ERROR:
            ret_string += QObject::tr("The decrypted file could not be written to!");
            break;

        case CRYPT_SUCCESS:
            ret_string += QObject::tr("The file or directory was successfully encrypted!");
            break;

        case DECRYPT_SUCCESS:
            ret_string += QObject::tr("The file or directory was successfully decrypted!");
            break;

        case ZIP_ERROR:
            ret_string += QObject::tr("The intermediate file could not be unzipped!");
            break;

        case NOT_STARTED:
            ret_string += QObject::tr("The file or directory was skipped!");
            break;

        case NOT_ARSENIC_FILE:
            ret_string += QObject::tr("The item wasn't a Arsenic file!");
            break;

    }

    return ret_string;
}

QString encryptString(QString plaintext, QString password, QString userName)
{
       /*
        Output format is:
          version    (4 bytes)
          salt       (10 bytes) for Argon2
          nonce1     (24 bytes) for ChaCha20Poly1305
          nonce2     (24 bytes) for AES-256/GCM
          nonce3     (24 bytes) for Serpent/GCM
          ciphertext
       */

    string pass = password.toStdString();

    // Copy input data to a buffer
    string clear = plaintext.toStdString();
    SecureVector<uint8_t> pt(clear.data(), clear.data()+clear.length());


    /*
    Version code is First 24 bits of SHA-256("Arsenic Cryptobox"), followed by 8 0 bits
    for later use as flags, etc if needed
    */
    const uint32_t CRYPTOBOX_VERSION_CODE = 0x2EC4993A;
    const size_t VERSION_CODE_LEN = 4;

    const size_t ARGON_OUTPUT_LEN = CIPHER_KEY_LEN *3;
    const size_t CRYPTOBOX_HEADER_LEN = VERSION_CODE_LEN + ARGON_SALT_LEN + CIPHER_IV_LEN*3;

    string add_data=APP_URL.toStdString();
    SecureVector<uint8_t> add(add_data.data(),add_data.data()+add_data.length());

    std::unique_ptr<RandomNumberGenerator> rng(new AutoSeeded_RNG);

    SecureVector<uint8_t> out_buf(CRYPTOBOX_HEADER_LEN + pt.size());
    for(size_t i = 0; i != VERSION_CODE_LEN; ++i)
      out_buf[i] = get_byte(i, CRYPTOBOX_VERSION_CODE);

    rng->randomize(&out_buf[VERSION_CODE_LEN], ARGON_SALT_LEN);

    // Generate 3*24 bytes random nonce and copy result in out_buf
    SecureVector<uint8_t> master_iv_buffer(rng->random_vec(CIPHER_IV_LEN*3));
    copy_mem(&out_buf[VERSION_CODE_LEN + ARGON_SALT_LEN], master_iv_buffer.data(), master_iv_buffer.size());

    if(pt.size() > 0)
       copy_mem(&out_buf[CRYPTOBOX_HEADER_LEN], pt.data(), pt.size());

    // Generate the 3*32 bytes master key by Argon2
    auto pwdhash_fam = PasswordHashFamily::create("Argon2id");
    SecureVector<uint8_t> master_key_buffer(ARGON_OUTPUT_LEN);

    auto default_pwhash = pwdhash_fam->from_params(MEMLIMIT_INTERACTIVE,ITERATION_INTERACTIVE,PARALLELISM_INTERACTIVE); // mem,ops,threads
    default_pwhash->derive_key(master_key_buffer.data(), master_key_buffer.size(),
                               pass.data(), pass.size(),
                               &out_buf[VERSION_CODE_LEN], ARGON_SALT_LEN);

    // Split master_key in tree parts.
    const uint8_t* mk = master_key_buffer.begin().base();
    const SymmetricKey cipher_key_1(mk, CIPHER_KEY_LEN);
    const SymmetricKey cipher_key_2(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey cipher_key_3(&mk[CIPHER_KEY_LEN+CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    // Split master_iv in tree parts.
    const uint8_t* iv = master_iv_buffer.begin().base();
    const InitializationVector iv_1(iv, CIPHER_IV_LEN);
    const InitializationVector iv_2(&iv[CIPHER_IV_LEN], CIPHER_IV_LEN);
    const InitializationVector iv_3(&iv[CIPHER_IV_LEN+CIPHER_IV_LEN], CIPHER_IV_LEN);

    // Now we can do the triple encryption
    std::unique_ptr<AEAD_Mode> enc = AEAD_Mode::create("ChaCha20Poly1305", ENCRYPTION);
    enc->set_key(cipher_key_1);
    enc->set_ad(add);
    enc->start(iv_1.bits_of());
    enc->finish(out_buf, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<AEAD_Mode> enc2 = AEAD_Mode::create("AES-256/EAX", ENCRYPTION);
    enc2->set_key(cipher_key_2);
    enc2->set_ad(add);
    enc2->start(iv_2.bits_of());
    enc2->finish(out_buf, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<AEAD_Mode> enc3 = AEAD_Mode::create("Serpent/GCM", ENCRYPTION);
    enc3->set_key(cipher_key_3);
    enc3->set_ad(add);
    enc3->start(iv_3.bits_of());
    enc3->finish(out_buf, CRYPTOBOX_HEADER_LEN);

    return QString::fromStdString(PEM_Code::encode(out_buf, "ARSENIC CRYPTOBOX MESSAGE"));
}

QString decryptString(QString cipher, QString password, QString userName)
{
    const string cipherStr=cipher.toStdString();
    const string pass=password.toStdString();

    string add_data=APP_URL.toStdString();
    SecureVector<uint8_t> add(add_data.data(),add_data.data()+add_data.length());

    const uint32_t CRYPTOBOX_VERSION_CODE = 0x2EC4993A;
    const size_t VERSION_CODE_LEN = 4;

    const size_t ARGON_OUTPUT_LEN = CIPHER_KEY_LEN *3;
    const size_t CRYPTOBOX_HEADER_LEN = VERSION_CODE_LEN + ARGON_SALT_LEN + CIPHER_IV_LEN *3;

    DataSource_Memory input_src(cipherStr);
    SecureVector<uint8_t> ciphertext =
       PEM_Code::decode_check_label(input_src,
                                    "ARSENIC CRYPTOBOX MESSAGE");

    if(ciphertext.size() < CRYPTOBOX_HEADER_LEN)
       throw Decoding_Error("Invalid CryptoBox input");

    for(size_t i = 0; i != VERSION_CODE_LEN; ++i)
       if(ciphertext[i] != get_byte(i, CRYPTOBOX_VERSION_CODE))
          throw Decoding_Error("Bad CryptoBox version");

    const uint8_t* argon_salt = &ciphertext[VERSION_CODE_LEN];
    const uint8_t* iv = &ciphertext[VERSION_CODE_LEN+ARGON_SALT_LEN];

    // mem,ops,threads
    auto pwdhash_fam = PasswordHashFamily::create("Argon2id");
    SecureVector<uint8_t> key_buffer(ARGON_OUTPUT_LEN);
    auto default_pwhash = pwdhash_fam->from_params(MEMLIMIT_INTERACTIVE,ITERATION_INTERACTIVE,PARALLELISM_INTERACTIVE);

    default_pwhash->derive_key(key_buffer.data(), key_buffer.size(),
                               pass.data(), pass.size(),
                               argon_salt, ARGON_SALT_LEN);

    // Split master_key in tree parts.
    const uint8_t* mk = key_buffer.begin().base();
    const Botan::SymmetricKey cipher_key_1(mk, CIPHER_KEY_LEN);
    const Botan::SymmetricKey cipher_key_2(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const Botan::SymmetricKey cipher_key_3(&mk[CIPHER_KEY_LEN+CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    // Split iv_buffer in tree parts.
    const Botan::InitializationVector iv_1(iv, CIPHER_IV_LEN);
    const Botan::InitializationVector iv_2(&iv[CIPHER_IV_LEN], CIPHER_IV_LEN);
    const Botan::InitializationVector iv_3(&iv[CIPHER_IV_LEN+CIPHER_IV_LEN], CIPHER_IV_LEN);

    std::unique_ptr<Botan::AEAD_Mode> dec = Botan::AEAD_Mode::create("Serpent/GCM", Botan::DECRYPTION);
    dec->set_key(cipher_key_3);
    dec->set_ad(add);
    dec->start(iv_3.bits_of());
    dec->finish(ciphertext, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<Botan::AEAD_Mode> dec2 = Botan::AEAD_Mode::create("AES-256/EAX", Botan::DECRYPTION);
    dec2->set_key(cipher_key_2);
    dec2->set_ad(add);
    dec2->start(iv_2.bits_of());
    dec2->finish(ciphertext, CRYPTOBOX_HEADER_LEN);

    std::unique_ptr<Botan::AEAD_Mode> dec3 = Botan::AEAD_Mode::create("ChaCha20Poly1305", Botan::DECRYPTION);
    dec3->set_key(cipher_key_1);
    dec3->set_ad(add);
    dec3->start(iv_1.bits_of());
    dec3->finish(ciphertext, CRYPTOBOX_HEADER_LEN);

    ciphertext.erase(ciphertext.begin(), ciphertext.begin() + CRYPTOBOX_HEADER_LEN);

    const string out(ciphertext.begin(),ciphertext.end()); //std::string out(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    return QString::fromStdString(out);

}
