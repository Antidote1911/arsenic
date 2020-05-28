#include "crypto.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringBuilder>
#include <QThread>

#include "constants.h"
#include "messages.h"

#include <QtGlobal>
#if defined(Q_OS_UNIX)
    #include <botan-2/botan/argon2.h>
    #include <botan-2/botan/aead.h>
    #include <botan-2/botan/pem.h>
    #include <botan-2/botan/loadstor.h>
    #include <botan-2/botan/sodium.h>
#endif

#if defined(Q_OS_WIN)
    #include "botan_all.h"
#endif

using namespace ARs;
using namespace Botan;
using namespace std;
using namespace MyCryptMessagesPublic;

Crypto_Thread::Crypto_Thread(QObject* parent)
    : QThread(parent)
{
}

void Crypto_Thread::setParam(bool direction,
                             QStringList filenames,
                             const QString password,
                             quint32 argonmem,
                             quint32 argoniter,
                             bool deletefile)
{
    m_filenames = filenames;
    m_password = password;
    m_direction = direction;
    m_deletefile = deletefile;

    if (argonmem == 0) {
        m_argonmem = MEMLIMIT_INTERACTIVE;
    }

    if (argonmem == 1) {
        m_argonmem = MEMLIMIT_MODERATE;
    }

    if (argonmem == 2) {
        m_argonmem = MEMLIMIT_SENSITIVE;
    }

    ////////////////////////////////////////////////
    if (argoniter == 0) {
        m_argoniter = ITERATION_INTERACTIVE;
    }

    if (argoniter == 1) {
        m_argoniter = ITERATION_MODERATE;
    }

    if (argoniter == 2) {
        m_argoniter = ITERATION_SENSITIVE;
    }
}

void Crypto_Thread::run()
{
    for (auto& inputFileName : m_filenames)
    {
        if (aborted) {
            aborted = true;
            Crypto_Thread::terminate();
            return;
        }

        if (m_direction == true) {
            emit statusMessage("");
            emit statusMessage(QDateTime::currentDateTime().toString(
                                   "dddd dd MMMM yyyy (hh:mm:ss)")
                               + " encryption of " + inputFileName);

            qint32 result = encrypt(inputFileName);
            emit statusMessage(errorCodeToString(result));

            if (aborted) { // Reset abort flag
                aborted = false;
                return;
            }
        }
        else {
            emit statusMessage("");
            emit statusMessage(QDateTime::currentDateTime().toString(
                                   "dddd dd MMMM yyyy (hh:mm:ss)")
                               + " decryption of " + inputFileName);
            int result = decrypt(inputFileName);
            emit statusMessage(errorCodeToString(result));

            if (aborted) { // Reset abort flag
                aborted = false;
            }
        }
    }
}

qint32 Crypto_Thread::encrypt(const QString src_path)
{
    emit updateProgress(src_path, 0);

    /* format is:
     * MAGIC_NUMBER
     * APP_VERSION
     * Argon memlimit
     * Argon iterations
     * original fileNameSize
     * Argon salt  (16 bytes)
     * ivChaCha20  (24 bytes)
     * ivAES       (24 bytes)
     * ivSerpent   (24 bytes)
     * encrypted internal_master_key  (originalFileName(fileNameSize)+
     *                                 originalSize(sizeof(qint64))+
     *                                 32 bytes keys *3
     *                                 + MACBYTES * 3)
     * encrypted file
     *
     *
     */

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);
    const auto filesize { src_info.size() };
    const auto fileName { src_info.fileName().toUtf8() };
    const auto fileNameSize { fileName.size() };

    auto fileN { convertStringToVectorquint8(fileName) };
    auto fileS { convertIntToVectorquint8(filesize) };

    // Convert the QString password to securevector
    const auto password { convertStringToVectorChar(m_password) };

    // Convert the QString additionnal data to quint8 vector
    const auto add { convertStringToVectorquint8(APP_URL) };

    // Randomize the 16 bytes salt and the three 24 bytes nonces
    AutoSeeded_RNG rng;
    auto argonSalt { rng.random_vec(ARGON_SALT_LEN) };
    auto ivChaCha20 { rng.random_vec(CIPHER_IV_LEN) };
    auto ivAES { rng.random_vec(CIPHER_IV_LEN) };
    auto ivSerpent { rng.random_vec(CIPHER_IV_LEN) };

    // Append the file name to the buffer and the 3 Random key, and split them in 3 for file encryption
    SecureVector<quint8> master_buffer(CIPHER_KEY_LEN * 3 + fileN.size() + sizeof(qint64));
    memcpy(master_buffer.data(), fileN.data(), fileN.size());
    memcpy(master_buffer.data() + fileN.size(), fileS.data(), sizeof(qint64));

    rng.randomize(master_buffer.data() + fileN.size() + sizeof(qint64), CIPHER_KEY_LEN * 3);

    const auto* mk { master_buffer.begin().base() };
    const OctetString name(mk, fileNameSize);
    const OctetString originalSize(&mk[fileNameSize], sizeof(qint64));
    const SymmetricKey cipherkey1(&mk[fileNameSize + sizeof(qint64)], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey2(&mk[fileNameSize + sizeof(qint64) + CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey3(&mk[fileNameSize + sizeof(qint64) + CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    // chained triple encryption of the internal_master_key with the random 3 master_key
    emit statusMessage("Argon2 passphrase derivation... Please wait.");
    const auto master_key { calculateHash(password, argonSalt, m_argonmem, m_argoniter) };
    const auto* mk2 { master_key.begin().base() };
    const SymmetricKey ChaCha20_key(mk2, CIPHER_KEY_LEN);
    const SymmetricKey AES_key(&mk2[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey Serpent_key(&mk2[CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    const auto master1 = AEAD_Mode::create("ChaCha20Poly1305", ENCRYPTION);
    const auto master2 = AEAD_Mode::create("AES-256/EAX", ENCRYPTION);
    const auto master3 = AEAD_Mode::create("Serpent/GCM", ENCRYPTION);

    master1->set_key(ChaCha20_key);
    master1->set_ad(add);
    master1->start(ivChaCha20);
    master1->finish(master_buffer);

    master2->set_key(AES_key);
    master2->set_ad(add);
    master2->start(ivAES);
    master2->finish(master_buffer);

    master3->set_key(Serpent_key);
    master3->set_ad(add);
    master3->start(ivSerpent);
    master3->finish(master_buffer);

    if (!src_file.exists() || !src_info.isFile()) {
        return(SRC_CANNOT_OPEN_READ);
    }

    if (!src_file.open(QIODevice::ReadOnly)) {
        return(SRC_CANNOT_OPEN_READ);
    }

    // create the new file, the path should normally be to the same directory as the source
    QFile des_file(QDir::cleanPath(src_path) + DEFAULT_EXTENSION);

    if (des_file.exists()) {
        return(DES_FILE_EXISTS);
    }

    if (!des_file.open(QIODevice::WriteOnly)) {
        return(DES_CANNOT_OPEN_WRITE);
    }

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // Write a "magic number" , arsenic version, argon2 parameters, etc...
    des_stream << static_cast<quint32>(MAGIC_NUMBER);
    des_stream << static_cast<QString>(APP_VERSION);
    des_stream << static_cast<quint32>(m_argonmem);
    des_stream << static_cast<quint32>(m_argoniter);
    des_stream << static_cast<quint32>(fileNameSize);

    // Write the salt, the 3 nonces and the encrypted header in the file
    des_stream.writeRawData(reinterpret_cast<char*>(argonSalt.data()), ARGON_SALT_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(ivChaCha20.data()), CIPHER_IV_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(ivAES.data()), CIPHER_IV_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(ivSerpent.data()), CIPHER_IV_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // now, move on to the actual data
    QDataStream src_stream(&src_file);
    emit statusMessage("Encryption... Please wait...");

    const auto enc1 { AEAD_Mode::create("ChaCha20Poly1305", ENCRYPTION) };
    const auto enc2 { AEAD_Mode::create("AES-256/EAX", ENCRYPTION) };
    const auto enc3 { AEAD_Mode::create("Serpent/GCM", ENCRYPTION) };

    Sodium::sodium_increment(ivChaCha20.data(), CIPHER_IV_LEN);
    enc1->set_key(cipherkey1);
    enc1->set_ad(add);
    enc1->start(ivChaCha20);

    Sodium::sodium_increment(ivAES.data(), CIPHER_IV_LEN);
    enc2->set_key(cipherkey2);
    enc2->set_ad(add);
    enc2->start(ivAES);

    Sodium::sodium_increment(ivSerpent.data(), CIPHER_IV_LEN);
    enc3->set_key(cipherkey3);
    enc3->set_ad(add);
    enc3->start(ivSerpent);

    auto processed { 0. };
    quint32 bytes_read;
    SecureVector<quint8> buf(IN_BUFFER_SIZE);

    while (!aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(buf.data()), IN_BUFFER_SIZE)) > 0)
    {
        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_path, (processed / filesize) * 100);

        if (bytes_read < IN_BUFFER_SIZE) {
            buf.resize(bytes_read);
            enc1->finish(buf);
            enc2->finish(buf);
            enc3->finish(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), buf.size());
        }
        else {
            enc1->update(buf);
            enc2->update(buf);
            enc3->update(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), bytes_read);
        }
    }

    if (aborted) {
        emit updateProgress(src_path, 0);
        des_file.close();
        des_file.remove();
        return(ABORTED_BY_USER);
    }

    if (m_deletefile) {
        src_file.close();
        QFile::remove(src_path);
        emit sourceDeletedAfterSuccess(src_path);
    }

    emit addEncrypted(des_file.fileName());
    return(CRYPT_SUCCESS);
}

qint32 Crypto_Thread::decrypt(QString src_path)
{
    emit updateProgress(src_path, 0);

    // Convert the QString password to char vector
    const auto password { convertStringToVectorChar(m_password) };

    // Convert the QString additionnal data to quint8 vector
    const auto add { convertStringToVectorquint8(APP_URL) };

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);

    if (!src_file.exists() || !src_info.isFile()) {
        emit statusMessage("Error: can't read the file");
        return(SRC_CANNOT_OPEN_READ);
    }

    if (!src_file.open(QIODevice::ReadOnly)) {
        emit statusMessage("Error: can't read the file");
        return(SRC_CANNOT_OPEN_READ);
    }

    // open the source file and extract all informations necessary for decryption
    QDataStream src_stream(&src_file);
    src_stream.setVersion(QDataStream::Qt_5_0);

    // Read and check the header
    quint32 magic;
    src_stream >> magic;

    if (magic != 0x41525345) {
        emit statusMessage("Error: this file is not an Arsenic file");
        return(NOT_AN_ARSENIC_FILE);
    }

    // Read and check the version
    QString version;
    src_stream >> version;
    emit statusMessage("this file is encrypted with Arsenic version :" + version);

    if (version < APP_VERSION) {
        emit statusMessage("Warning: this is file is encrypted by an old Arsenic Version...");
        emit statusMessage("Warning: version of encrypted file: " + version);
        emit statusMessage("Warning: version of your Arsenic: " + APP_VERSION);
    }

    if (version > APP_VERSION) {
        emit statusMessage("Warning: this file is encrypted with a more recent "
                           "version of Arsenic...");
        emit statusMessage("Warning: version of encrypted file: " + version);
        emit statusMessage("Warning: version of your Arsenic: " + APP_VERSION);
    }

    // Read Argon2 parameters
    quint32 memlimit;
    src_stream >> memlimit;

    quint32 iterations;
    src_stream >> iterations;

    quint32 fileNameSize;
    src_stream >> fileNameSize;

    SecureVector<quint8> salt_buffer(ARGON_SALT_LEN);
    SecureVector<quint8> ivChaCha20(CIPHER_IV_LEN);
    SecureVector<quint8> ivAES(CIPHER_IV_LEN);
    SecureVector<quint8> ivSerpent(CIPHER_IV_LEN);
    SecureVector<quint8> master_buffer(fileNameSize + sizeof(qint64) + CIPHER_KEY_LEN * 3 + MACBYTES * 3);

    // Read the salt, the three nonces and the header
    src_stream.readRawData(reinterpret_cast<char*>(salt_buffer.data()), ARGON_SALT_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(ivChaCha20.data()), CIPHER_IV_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(ivAES.data()), CIPHER_IV_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(ivSerpent.data()), CIPHER_IV_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // calculate the internal key with Argon2 and split them in three
    emit statusMessage("Argon2 passphrase derivation... Please wait.");
    const auto master_key { calculateHash(password, salt_buffer, memlimit, iterations) };

    const auto* mk { master_key.begin().base() };
    const SymmetricKey ChaCha20_key(mk, CIPHER_KEY_LEN);
    const SymmetricKey AES_key(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey Serpent_key(&mk[CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    // decrypt header
    const auto master1 { AEAD_Mode::create("Serpent/GCM", DECRYPTION) };
    const auto master2 { AEAD_Mode::create("AES-256/EAX", DECRYPTION) };
    const auto master3 { AEAD_Mode::create("ChaCha20Poly1305", DECRYPTION) };
    try {
        master1->set_key(Serpent_key);
        master1->set_ad(add);
        master1->start(ivSerpent);
        master1->finish(master_buffer);

        master2->set_key(AES_key);
        master2->set_ad(add);
        master2->start(ivAES);
        master2->finish(master_buffer);

        master3->set_key(ChaCha20_key);
        master3->set_ad(add);
        master3->start(ivChaCha20);
        master3->finish(master_buffer);
    } catch (const Botan::Invalid_Authentication_Tag&) {
        QFile::remove(outfileresult);
        return(SRC_HEADER_INVALID_TAG);
    } catch (const Botan::Integrity_Failure&) {
        QFile::remove(outfileresult);
        return(SRC_HEADER_INTEGRITY_FAILURE);
    }
    // get from the decrypted header the three internal keys and the original filename
    const auto* mk2 { master_buffer.begin().base() };
    const OctetString name(mk2, fileNameSize);
    const OctetString originalSize(&mk2[fileNameSize], sizeof(qint64));
    const SymmetricKey cipherkey1(&mk2[fileNameSize + sizeof(qint64)], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey2(&mk2[fileNameSize + sizeof(qint64) + CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey3(&mk2[fileNameSize + sizeof(qint64) + CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    // create the decrypted file
    const string tmp { (name.begin()), name.end() };
    const auto originalName { QString::fromStdString(tmp) };
    QFile des_file(uniqueFileName(QDir::cleanPath(src_info.absolutePath() + "/" + originalName)));

    if (!des_file.open(QIODevice::WriteOnly)) {
        emit statusMessage("Error: the output file can't be open for writing");
        return(DES_CANNOT_OPEN_WRITE);
    }

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // start decrypting the actual data
    // decrypt the file and write the plaintext to the destination.

    emit statusMessage("Decryption... Please wait.");

    const auto decSerpent { AEAD_Mode::create("Serpent/GCM", DECRYPTION) };
    const auto decAES { AEAD_Mode::create("AES-256/EAX", DECRYPTION) };
    const auto decChaCha { AEAD_Mode::create("ChaCha20Poly1305", DECRYPTION) };

    Sodium::sodium_increment(ivSerpent.data(), CIPHER_IV_LEN);
    decSerpent->set_key(cipherkey3);
    decSerpent->set_ad(add);
    decSerpent->start(ivSerpent);

    Sodium::sodium_increment(ivAES.data(), CIPHER_IV_LEN);
    decAES->set_key(cipherkey2);
    decAES->set_ad(add);
    decAES->start(ivAES);

    Sodium::sodium_increment(ivChaCha20.data(), CIPHER_IV_LEN);
    decChaCha->set_key(cipherkey1);
    decChaCha->set_ad(add);
    decChaCha->start(ivChaCha20);

    //The percentage is calculated by dividing the progress (value() - minimum()) divided by maximum() - minimum().
    auto processed { 0. };
    quint32 bytes_read;
    SecureVector<quint8> buf(IN_BUFFER_SIZE);

    const string size { (originalSize.begin()), originalSize.end() };
    auto originalFileSize = std::stoi(size);

    while (!aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(buf.data()), IN_BUFFER_SIZE)) > 0)
    {
        // calculate percentage proccessed
        processed += bytes_read - MACBYTES * 3;
        emit updateProgress(src_path, (processed / originalFileSize) * 100);

        if (bytes_read < IN_BUFFER_SIZE) {
            buf.resize(bytes_read);
            try {
                decSerpent->finish(buf);
                decAES->finish(buf);
                decChaCha->finish(buf);
                des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), buf.size());
            } catch (const Botan::Invalid_Authentication_Tag&) {
                QFile::remove(outfileresult);
                return(INVALID_TAG);
            } catch (const Botan::Integrity_Failure&) {
                QFile::remove(outfileresult);
                return(INTEGRITY_FAILURE);
            }
            emit updateProgress(src_path, 100);
        }
        else {
            decSerpent->update(buf);
            decAES->update(buf);
            decChaCha->update(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), bytes_read);
        }
    }

    if (m_deletefile) {
        src_file.close();
        QFile::remove(src_path);
        emit sourceDeletedAfterSuccess(src_path);
    }

    QFileInfo des_info(des_file);
    emit addDecrypted(des_info.filePath());
    return(DECRYPT_SUCCESS);
}

SecureVector<char> Crypto_Thread::convertStringToVectorChar(QString qstring)
{
    const auto pass { qstring.toStdString() };
    SecureVector<char> passv(pass.begin(), pass.end());
    return(passv);
}

SecureVector<char> Crypto_Thread::convertStringToVectorChar(string string)
{
    SecureVector<char> passv(string.begin(), string.end());
    return(passv);
}

SecureVector<quint8> Crypto_Thread::convertStringToVectorquint8(QString qstring)
{
    const auto pass { qstring.toStdString() };
    SecureVector<quint8> passv(pass.begin(), pass.end());
    return(passv);
}

SecureVector<quint8> Crypto_Thread::convertStringToVectorquint8(string string)
{
    SecureVector<quint8> passv(string.begin(), string.end());
    return(passv);
}

SecureVector<quint8> Crypto_Thread::convertIntToVectorquint8(qint64 num)
{
    auto s = std::to_string(num);
    SecureVector<quint8> v(s.begin(), s.end());
    return(v);
}

SecureVector<quint8> Crypto_Thread::calculateHash(SecureVector<char> pass_buffer,
                                                  Botan::SecureVector<quint8> salt_buffer,
                                                  quint32 memlimit,
                                                  quint32 iterations)
{
    auto pwdhash_fam { PasswordHashFamily::create("Argon2id") };
    SecureVector<quint8> key_buffer(CIPHER_KEY_LEN * 3);

    // mem,ops,threads
    auto default_pwhash { pwdhash_fam->from_params(memlimit, iterations, PARALLELISM_INTERACTIVE) };

    default_pwhash->derive_key(key_buffer.data(),
                               key_buffer.size(),
                               pass_buffer.data(),
                               pass_buffer.size(),
                               salt_buffer.data(),
                               salt_buffer.size());
    return(key_buffer);
}

QString Crypto_Thread::removeExtension(const QString& fileName,
                                       const QString& extension)
{
    QFileInfo file { fileName };
    auto newFileName = fileName;

    if (file.suffix() == extension) {
        newFileName = file.absolutePath() % QDir::separator() % file.completeBaseName();
    }

    return(newFileName);
}

QString Crypto_Thread::uniqueFileName(const QString& fileName)
{
    QFileInfo originalFile { fileName };
    auto uniqueFileName { fileName };

    auto foundUniqueFileName { false };
    auto i { 0 };

    while (!foundUniqueFileName && i < 100000)
    {
        QFileInfo uniqueFile { uniqueFileName };

        if (uniqueFile.exists() && uniqueFile.isFile()) { // Write number of copies before file extension
            uniqueFileName = originalFile.absolutePath() % QDir::separator() % originalFile.baseName() % QString { " (%1)" }.arg(i + 2);

            if (!originalFile.completeSuffix().isEmpty()) { // Add the file extension if there is one
                uniqueFileName += QStringLiteral(".") % originalFile.completeSuffix();
            }

            ++i;
        }
        else {
            foundUniqueFileName = true;
        }
    }
    return(uniqueFileName);
}
QString Crypto_Thread::errorCodeToString(int error_code)
{
    QString ret_string;

    switch (error_code)
    {
    case NOT_AN_ARSENIC_FILE:
        ret_string += tr("This file is not an Arsenic File !");
        break;

    case SRC_NOT_FOUND:
        ret_string += tr("The file was not found !");
        break;

    case SRC_CANNOT_OPEN_READ:
        ret_string += tr("The file could not be opened for reading !");
        break;

    case PASS_HASH_FAIL:
        ret_string += tr("The password could not be hashed !");
        break;

    case DES_HEADER_ENCRYPT_ERROR:
        ret_string += tr("The intermediate file header could not be encrypted !");
        break;

    case DES_FILE_EXISTS:
        ret_string += tr("The encrypted file already exists !");
        break;

    case DES_CANNOT_OPEN_WRITE:
        ret_string += tr("The encrypted file could not be opened for writin !");
        break;

    case DES_HEADER_WRITE_ERROR:
        ret_string += tr("The encrypted file could not be written to !");
        break;

    case SRC_BODY_READ_ERROR:
        ret_string += tr("The file could not be read !");
        break;

    case DATA_ENCRYPT_ERROR:
        ret_string += tr("The file's data could not be encrypted !");
        break;

    case DES_BODY_WRITE_ERROR:
        ret_string += tr("The encrypted file could not be written to !");
        break;

    case SRC_HEADER_READ_ERROR:
        ret_string += tr("Can't read the header !");
        break;

    case SRC_HEADER_INVALID_TAG:
        ret_string += tr("Invalid Authentication Tag. could not decrypt the header. Incorrect password or corrupted file !");
        break;

    case SRC_HEADER_INTEGRITY_FAILURE:
        ret_string += tr("Header Integrity Failure. Incorrect password or corrupted file !");
        break;

    case INVALID_TAG:
        ret_string += tr("Invalid Authentication Tag. The file is corrupted !");
        break;

    case INTEGRITY_FAILURE:
        ret_string += tr("File Integrity Failure. The file is corrupted !");
        break;

    case ABORTED_BY_USER:
        ret_string += tr("Aborted by user ! Intermediate file is deleted !");
        break;

    case CRYPT_SUCCESS:
        ret_string += tr("The file was successfully encrypted !");
        break;

    case DECRYPT_SUCCESS:
        ret_string += tr("The file was successfully decrypted !");
        break;
    }

    return(ret_string);
}
