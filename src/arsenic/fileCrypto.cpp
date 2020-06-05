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
        if (m_aborted) {
            m_aborted = true;
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

            if (m_aborted) { // Reset abort flag
                m_aborted = false;
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

            if (m_aborted) { // Reset abort flag
                m_aborted = false;
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
     * original fileSize
     * Argon salt  (16 bytes)
     * ivChaCha20 +  ivAES +  ivSerpent (24 bytes *3)
     * encrypted HeaderOriginalFileName  ( fileNameSize + randomBloc(IN_BUFFER_SIZE) + MACBYTES*3 )
     * encrypted dataBlock1  ( IN_BUFFER_SIZE + MACBYTES*3 )
     * encrypted dataBlock2  ( IN_BUFFER_SIZE + MACBYTES*3 )
     * ...
     * ...
     */

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);
    const auto fileSize { src_info.size() };

    const auto fileName = src_info.fileName().toUtf8();
    const auto fileNameSize = fileName.size();

    AutoSeeded_RNG rng;
    const auto randomData { rng.random_vec(IN_BUFFER_SIZE) };
    auto argonSalt { rng.random_vec(ARGON_SALT_LEN) };
    auto tripleNonce { rng.random_vec(CIPHER_IV_LEN * 3) };

    // Append the file name to the buffer and some random data
    SecureVector<quint8> master_buffer(fileName.size() + randomData.size());
    memcpy(master_buffer.data(), fileName.data(), fileNameSize);
    memcpy(master_buffer.data() + fileNameSize, randomData.data(), randomData.size());

    // Triple encryption of the buffer who contain the original name of the file
    // and some random data

    TripleEncryption encrypt(0);
    encrypt.setSalt(argonSalt);
    emit statusMessage("Argon2 passphrase derivation... Please wait.");
    encrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    encrypt.setTripleNonce(tripleNonce);
    encrypt.finish(master_buffer);

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
    des_stream << static_cast<qint64>(fileNameSize);
    des_stream << static_cast<qint64>(fileSize);

    // Write the salt, the 3 nonces and the encrypted header in the file
    des_stream.writeRawData(reinterpret_cast<char*>(argonSalt.data()), ARGON_SALT_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(tripleNonce.data()), CIPHER_IV_LEN * 3);
    des_stream.writeRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // now, move on to the actual data
    QDataStream src_stream(&src_file);
    emit statusMessage("Encryption... Please wait...");

    SecureVector<quint8> encryptedBloc;

    auto processed { 0. };
    quint32 bytes_read;
    SecureVector<quint8> inBuf(IN_BUFFER_SIZE);
    SecureVector<quint8> outBuf;

    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), IN_BUFFER_SIZE)) > 0) //65536
    {
        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_path, (processed / fileSize) * 100);

        outBuf = inBuf;
        encrypt.finish(outBuf);
        des_stream.writeRawData(reinterpret_cast<char *>(outBuf.data()), outBuf.size()); // 65584
    }

    if (m_aborted) {
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

    qint64 fileNameSize;
    src_stream >> fileNameSize;

    qint64 originalfileSize;
    src_stream >> originalfileSize;

    // On most systems the maximum filename length is 255 bytes
    if (fileNameSize > 255) {
        return(SRC_HEADER_READ_ERROR);
    }

    SecureVector<quint8> salt_buffer(ARGON_SALT_LEN);
    SecureVector<quint8> tripleNonce(CIPHER_IV_LEN * 3);
    SecureVector<quint8> master_buffer(fileNameSize + IN_BUFFER_SIZE + MACBYTES * 3);


    // Read the salt, the three nonces and the header
    if (!src_stream.readRawData(reinterpret_cast<char*>(salt_buffer.data()), ARGON_SALT_LEN)) {
        return(SRC_HEADER_READ_ERROR);
    }

    if (!src_stream.readRawData(reinterpret_cast<char*>(tripleNonce.data()), CIPHER_IV_LEN * 3)) {
        return(SRC_HEADER_READ_ERROR);
    }

    if (!src_stream.readRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size())) {
        return(SRC_HEADER_READ_ERROR);
    }

    // calculate the internal key with Argon2 and split them in three
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    // decrypt header
    TripleEncryption decrypt(1);
    decrypt.setSalt(salt_buffer);
    decrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    decrypt.setTripleNonce(tripleNonce);
    try {
        decrypt.finish(master_buffer);
    } catch (const Botan::Invalid_Authentication_Tag&) {
        return(INVALID_TAG);
    } catch (const Botan::Integrity_Failure&) {
        return(INTEGRITY_FAILURE);
    }catch (const Botan::Decoding_Error&) {
        return(INTEGRITY_FAILURE);
    }

    // get from the decrypted header the three internal keys and the original filename
    const auto* mk2 { master_buffer.begin().base() };
    const OctetString name(mk2, fileNameSize);

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
    SecureVector<quint8> decryptedBloc;

    //The percentage is calculated by dividing the progress (value() - minimum()) divided by maximum() - minimum().
    auto processed { 0. };
    quint32 bytes_read;
    SecureVector<quint8> inBuf(IN_BUFFER_SIZE + MACBYTES * 3);
    SecureVector<quint8> outBuf;

    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), IN_BUFFER_SIZE + MACBYTES * 3)) > 0)
    {
        // calculate percentage proccessed
        processed += bytes_read - MACBYTES * 3;
        emit updateProgress(src_path, (processed / originalfileSize) * 100);

        try {
            outBuf = inBuf;
            decrypt.finish(outBuf);
            des_stream.writeRawData(reinterpret_cast<char *>(outBuf.data()), outBuf.size());
        } catch (const Botan::Invalid_Authentication_Tag&) {
            des_file.remove();
            return(INVALID_TAG);
        } catch (const Botan::Integrity_Failure&) {
            des_file.remove();
            return(INTEGRITY_FAILURE);
        }catch (const Botan::Decoding_Error&)
        {
            return(INTEGRITY_FAILURE);
        }
        emit updateProgress(src_path, 100);
    }

    if (m_deletefile) {
        src_file.close();
        src_file.remove();
        emit sourceDeletedAfterSuccess(src_path);
    }

    QFileInfo des_info(des_file);
    emit addDecrypted(des_info.filePath());
    return(DECRYPT_SUCCESS);
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

void Crypto_Thread::abort()
{
    m_aborted = true;
}
