#include "fileCrypto.h"

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
#include "botan_all.h"
#include "tripleencryption.h"

using namespace ARs;
using namespace Botan;
using namespace std;

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

    // Append the file name to the buffer and the 3 Random key, and split them in 3 for file encryption
    AutoSeeded_RNG rng;
    auto masterKey { rng.random_vec(CIPHER_KEY_LEN * 3) };
    auto internalTripleNonce { rng.random_vec(CIPHER_IV_LEN * 3) };

    SecureVector<quint8> master_buffer(CIPHER_KEY_LEN * 3 + fileN.size() + sizeof(qint64) + CIPHER_IV_LEN * 3);
    memcpy(master_buffer.data(), fileN.data(), fileN.size());
    memcpy(master_buffer.data() + fileN.size(), fileS.data(), sizeof(qint64));
    memcpy(master_buffer.data() + fileN.size() + sizeof(qint64), masterKey.data(), masterKey.size());
    memcpy(master_buffer.data() + fileN.size() + sizeof(qint64) + masterKey.size(), internalTripleNonce.data(), internalTripleNonce.size());

    const auto* mk { master_buffer.begin().base() };
    const OctetString name(mk, fileNameSize);
    const OctetString originalSize(&mk[fileNameSize], sizeof(qint64));

    // chained triple encryption of the internal_master_key with the random 3 master_key
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    // Randomize the 16 bytes salt and the three 24 bytes nonces

    TripleEncryption encrypt(0);
    encrypt.generateSalt();
    encrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    encrypt.setAdd(APP_URL);
    encrypt.generateTripleNonce();
    SecureVector<quint8> outbuffer = encrypt.finish(master_buffer);

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
    des_stream << static_cast<QVersionNumber>(APP_VERSION);
    des_stream << static_cast<quint32>(m_argonmem);
    des_stream << static_cast<quint32>(m_argoniter);
    des_stream << static_cast<quint32>(fileNameSize);

    // Write the salt, the 3 nonces and the encrypted header in the file
    des_stream.writeRawData(reinterpret_cast<char*>(encrypt.getSalt().bits_of().data()), ARGON_SALT_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(encrypt.getTripleNonce().bits_of().data()), CIPHER_IV_LEN * 3);
    des_stream.writeRawData(reinterpret_cast<char*>(outbuffer.data()), outbuffer.size());

    // now, move on to the actual data
    QDataStream src_stream(&src_file);
    emit statusMessage("Encryption... Please wait...");

    TripleEncryption encrypt2(0);
    encrypt2.setTripleKey(masterKey);
    encrypt2.setAdd(APP_URL);

    encrypt2.setTripleNonce(internalTripleNonce);
    SecureVector<quint8> encryptedBloc;

    auto processed { 0. };
    quint32 bytes_read;
    SecureVector<quint8> buf(IN_BUFFER_SIZE);

    while (!aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(buf.data()), IN_BUFFER_SIZE)) > 0)
    {
        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_path, (processed / filesize) * 100);

        encryptedBloc = encrypt2.finish(buf);
        des_stream.writeRawData(reinterpret_cast<char*>(encryptedBloc.data()), encryptedBloc.size());
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
    QVersionNumber version;
    src_stream >> version;
    emit statusMessage("this file is encrypted with Arsenic version :" + version.toString());

    if (version < APP_VERSION) {
        emit statusMessage("Warning: this is file is encrypted by an old Arsenic Version...");
        emit statusMessage("Warning: version of encrypted file: " + version.toString());
        emit statusMessage("Warning: version of your Arsenic: " + APP_VERSION.toString());
    }

    if (version > APP_VERSION) {
        emit statusMessage("Warning: this file is encrypted with a more recent "
                           "version of Arsenic...");
        emit statusMessage("Warning: version of encrypted file: " + version.toString());
        emit statusMessage("Warning: version of your Arsenic: " + APP_VERSION.toString());
    }

    // Read Argon2 parameters
    quint32 memlimit;
    src_stream >> memlimit;

    quint32 iterations;
    src_stream >> iterations;

    quint32 fileNameSize;
    src_stream >> fileNameSize;

    SecureVector<quint8> salt_buffer(ARGON_SALT_LEN);
    SecureVector<quint8> tripleNonce(CIPHER_IV_LEN * 3);
    SecureVector<quint8> master_buffer(fileNameSize + sizeof(qint64) + CIPHER_KEY_LEN * 3 + CIPHER_IV_LEN * 3 + MACBYTES * 3);

    // Read the salt, the three nonces and the header
    src_stream.readRawData(reinterpret_cast<char*>(salt_buffer.data()), ARGON_SALT_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(tripleNonce.data()), CIPHER_IV_LEN * 3);
    src_stream.readRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // calculate the internal key with Argon2 and split them in three
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    // decrypt header
    TripleEncryption decrypt(1);
    decrypt.setSalt(salt_buffer);
    decrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    decrypt.setAdd(APP_URL);
    decrypt.setTripleNonce(tripleNonce);
    SecureVector<quint8> outbuffer;
    try {
        outbuffer = decrypt.finish(master_buffer);
    } catch (const Botan::Invalid_Authentication_Tag&) {
        return(INVALID_TAG);
    } catch (const Botan::Integrity_Failure&) {
        return(INTEGRITY_FAILURE);
    }catch (const Botan::Decoding_Error&) {
        return(INTEGRITY_FAILURE);
    }

    // get from the decrypted header the three internal keys and the original filename
    const auto* mk2 { outbuffer.begin().base() };
    const OctetString name(mk2, fileNameSize);
    const OctetString originalSize(&mk2[fileNameSize], sizeof(qint64));
    const SymmetricKey cipherkey(&mk2[fileNameSize + sizeof(qint64)], CIPHER_KEY_LEN * 3);
    const InitializationVector internalTripleNonce(&mk2[fileNameSize + sizeof(qint64) + CIPHER_KEY_LEN * 3], CIPHER_IV_LEN * 3);

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

    TripleEncryption decrypt2(1);
    decrypt2.setTripleKey(cipherkey);
    decrypt2.setAdd(APP_URL);
    decrypt2.setTripleNonce(internalTripleNonce);
    SecureVector<quint8> decryptedBloc;

    //The percentage is calculated by dividing the progress (value() - minimum()) divided by maximum() - minimum().
    auto processed { 0. };
    quint32 bytes_read;
    SecureVector<quint8> buf(IN_BUFFER_SIZE + MACBYTES * 3);

    const string size { (originalSize.begin()), originalSize.end() };
    auto originalFileSize = std::stoi(size);

    while (!aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(buf.data()), IN_BUFFER_SIZE + MACBYTES * 3)) > 0)
    {
        // calculate percentage proccessed
        processed += bytes_read - MACBYTES * 3;
        emit updateProgress(src_path, (processed / originalFileSize) * 100);

        try {
            decryptedBloc = decrypt2.finish(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(decryptedBloc.data()), decryptedBloc.size());
        } catch (const Botan::Invalid_Authentication_Tag&) {
            QFile::remove(outfileresult);
            return(INVALID_TAG);
        } catch (const Botan::Integrity_Failure&) {
            QFile::remove(outfileresult);
            return(INTEGRITY_FAILURE);
        }catch (const Botan::Decoding_Error&)
        {
            return(INTEGRITY_FAILURE);
        }
        emit updateProgress(src_path, 100);
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
