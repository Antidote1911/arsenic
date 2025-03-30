#include "CryptoThread.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringBuilder>
#include <QThread>
#include <QCoreApplication>

#include "botan_all.h"
#include "messages.h"
#include "triplecryptoengine.h"
#include "simplecryptoengine.h"
#include "utils.h"
#include <iostream>

using namespace Botan;
using namespace std;

Crypto_Thread::Crypto_Thread(QObject* parent)
    : QThread(parent)
{
}

void Crypto_Thread::setParam(bool direction,
                             QStringList const& filenames,
                             const QString& password,
                             const QString& algo,
                             quint32 argonmem,
                             quint32 argoniter,
                             bool deletefile)
{
    m_filenames  = filenames;
    m_password   = password;
    m_algo       = algo;
    m_direction  = direction;
    m_deletefile = deletefile;

    if (argonmem == 0)
        m_argonmem = m_const->MEMLIMIT_INTERACTIVE;
    if (argonmem == 1)
        m_argonmem = m_const->MEMLIMIT_MODERATE;
    if (argonmem == 2)
        m_argonmem = m_const->MEMLIMIT_SENSITIVE;

    ////////////////////////////////////////////////

    if (argoniter == 0)
        m_argoniter = m_const->ITERATION_INTERACTIVE;

    if (argoniter == 1)
        m_argoniter = m_const->ITERATION_MODERATE;

    if (argoniter == 2)
        m_argoniter = m_const->ITERATION_SENSITIVE;
}

void Crypto_Thread::run()
{

    for (auto& inputFileName : m_filenames) {
        if (m_aborted) {
            m_aborted = true;
            Crypto_Thread::terminate();
            return;
        }

        QFile src_file(QDir::cleanPath(inputFileName));
        QFileInfo src_info(src_file);
        if (!src_file.exists() || !src_info.isFile()) {
            emit statusMessage("SRC_CANNOT_OPEN_READ");
            return;
        }

        if (!src_file.open(QIODevice::ReadOnly)) {
            emit statusMessage("SRC_CANNOT_OPEN_READ");
            return;
        }
        quint32 result = 0;
        if (m_direction == true) {
            emit statusMessage("");
            emit statusMessage(QDateTime::currentDateTime().toString("dddd dd MMMM yyyy (hh:mm:ss)") + " encryption of " + inputFileName);

            if (m_algo=="Triple Encryption"){
                result         = tripleEncrypt(inputFileName);
            }else{
                result         = simpleEncrypt(inputFileName);
            }


            emit statusMessage(errorCodeToString(result));

            if (m_aborted) {
                m_aborted = false; // Reset abort flag
                return;
            }
        }
        else {
            emit statusMessage("");
            emit statusMessage(QDateTime::currentDateTime().toString("dddd dd MMMM yyyy (hh:mm:ss)") + " decryption of " + inputFileName);

            //check if it's a triple encryped file
            auto algo = Utils::getAlgo(src_file);
            if (algo == "Triple Encryption"){
                result         = tripleDecrypt(inputFileName);
            }else{
                result         = simpleDecrypt(inputFileName);
            }
            emit statusMessage(errorCodeToString(result));

            if (m_aborted) {
                m_aborted = false; // Reset abort flag
            }
        }
    }
}

quint32 Crypto_Thread::tripleEncrypt(const QString& src_path)
{

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
    const auto fileSize     = src_info.size();
    const auto fileName     = src_info.fileName().toUtf8();
    const auto fileNameSize = fileName.size();

    AutoSeeded_RNG rng;
    SecureVector<quint8> argonSalt   = rng.random_vec(m_const->ARGON_SALT_LEN);
    auto tripleNonce = rng.random_vec(m_const->CIPHER_IV_LEN * 3);

    const auto randomData = rng.random_vec(m_const->IN_BUFFER_SIZE);

    // Append the file name to the buffer and some random data
    SecureVector<quint8> master_buffer(fileName.size() + randomData.size());
    memcpy(master_buffer.data(), fileName.data(), fileNameSize);
    memcpy(master_buffer.data() + fileNameSize, randomData.data(), randomData.size());

    // encryption of the buffer who contain the original name of the file
    // and some random data

    TripleCryptoEngine encrypt(true);
    encrypt.setSalt(argonSalt);
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    encrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    encrypt.setNonce(tripleNonce);
    encrypt.finish(master_buffer);

    if (!src_file.exists() || !src_info.isFile())
        return (SRC_CANNOT_OPEN_READ);
    if (!src_file.open(QIODevice::ReadOnly))
        return (SRC_CANNOT_OPEN_READ);

    // create the new file.
    QFile des_file(Utils::uniqueFileName(src_path + m_const->DEFAULT_EXTENSION));
    //QFile des_file(src_path + m_const->DEFAULT_EXTENSION);

    if (des_file.exists())
        return (DES_FILE_EXISTS);
    if (!des_file.open(QIODevice::WriteOnly))
        return (DES_CANNOT_OPEN_WRITE);

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // Write a "magic number" , arsenic version, argon2 parameters, etc...
    des_stream << static_cast<quint32>(m_const->MAGIC_NUMBER);
    des_stream << static_cast<QVersionNumber>(m_const->APP_VERSION);
    des_stream << static_cast<quint32>(m_argonmem);
    des_stream << static_cast<quint32>(m_argoniter);
    des_stream << static_cast<QString>(m_algo);
    des_stream << static_cast<qint64>(fileNameSize);
    des_stream << static_cast<qint64>(fileSize);

    // Write the salt, the 3 nonces and the encrypted header in the file
    des_stream.writeRawData(reinterpret_cast<char*>(argonSalt.data()), m_const->ARGON_SALT_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(tripleNonce.data()), m_const->CIPHER_IV_LEN * 3);
    des_stream.writeRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // now, move on to the actual data
    QDataStream src_stream(&src_file);
    auto processed  = 0.;
    auto bytes_read = 0;
    SecureVector<quint8> inBuf(m_const->IN_BUFFER_SIZE);

    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), m_const->IN_BUFFER_SIZE)) > 0) {
        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_info.filePath(), (processed / fileSize) * 100);
        // ...
        inBuf.resize(bytes_read);
        encrypt.finish(inBuf);
        des_stream.writeRawData(reinterpret_cast<char*>(inBuf.data()), inBuf.size());
        inBuf.resize(m_const->IN_BUFFER_SIZE);
    }

    if (m_aborted) {
        emit updateProgress(src_info.filePath(), 0);
        des_file.close();
        des_file.remove();
        return (ABORTED_BY_USER);
    }

    if (m_deletefile) {
        src_file.close();
        QFile::remove(src_file.fileName());
        emit deletedAfterSuccess(src_path);
    }
    emit addEncrypted(des_file.fileName());
    return (CRYPT_SUCCESS);
}

quint32 Crypto_Thread::tripleDecrypt(const QString& src_path)
{
    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);
    QString absolutePath = src_info.absolutePath();

    if (!src_file.exists() || !src_info.isFile())
        return (SRC_CANNOT_OPEN_READ);

    if (!src_file.open(QIODevice::ReadOnly))
        return (SRC_CANNOT_OPEN_READ);

    // open the source file and extract all informations necessary for decryption
    QDataStream src_stream(&src_file);
    src_stream.setVersion(QDataStream::Qt_5_0);

    // Read and check the header
    quint32 magic;
    src_stream >> magic;

    if (magic != 0x41525345)
        return (NOT_AN_ARSENIC_FILE);

    // Read and check the version
    QVersionNumber version;
    src_stream >> version;
    emit statusMessage("this file is encrypted with Arsenic version " + version.toString());

    if (version < m_const->APP_VERSION) {
        emit statusMessage("Warning: this is file is encrypted by an old Arsenic Version...");
        emit statusMessage("Warning: version of encrypted file " + version.toString());
        emit statusMessage("Warning: version of your Arsenic " + m_const->APP_VERSION.toString());
    }

    if (version > m_const->APP_VERSION) {
        emit statusMessage("Warning: this file is encrypted with a more recent version of Arsenic...");
        emit statusMessage("Warning: version of encrypted file " + version.toString());
        emit statusMessage("Warning: version of your Arsenic " + m_const->APP_VERSION.toString());
    }

    // Read Argon2 parameters
    quint32 memlimit;
    src_stream >> memlimit;

    quint32 iterations;
    src_stream >> iterations;

    QString algo;
    src_stream >> algo;

    qint64 fileNameSize;
    src_stream >> fileNameSize;

    // On most systems the maximum filename length is 255 bytes
    if (fileNameSize > 255)
        return (SRC_HEADER_READ_ERROR);

    qint64 originalfileSize;
    src_stream >> originalfileSize;

    SecureVector<quint8> salt_buffer(m_const->ARGON_SALT_LEN);
    SecureVector<quint8> tripleNonce(m_const->CIPHER_IV_LEN * 3);
    SecureVector<quint8> master_buffer(fileNameSize + m_const->IN_BUFFER_SIZE + m_const->MACBYTES * 3);

    // Read the salt, the three nonces and the header
    if (!src_stream.readRawData(reinterpret_cast<char*>(salt_buffer.data()), m_const->ARGON_SALT_LEN))
        return (SRC_HEADER_READ_ERROR);

    if (!src_stream.readRawData(reinterpret_cast<char*>(tripleNonce.data()), m_const->CIPHER_IV_LEN * 3))
        return (SRC_HEADER_READ_ERROR);

    if (!src_stream.readRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size()))
        return (SRC_HEADER_READ_ERROR);

    // calculate the internal key with Argon2 and split them in three
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    // decrypt header
    TripleCryptoEngine decrypt(false);
    decrypt.setSalt(salt_buffer);
    decrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    decrypt.setNonce(tripleNonce);
    try {
        decrypt.finish(master_buffer);
    }
    catch (const Botan::Exception&) {
        return (DECRYPT_FAIL);
    }

    // get from the decrypted header the three internal keys and the original filename
    const quint8* mk2 = &master_buffer[0];
    const OctetString name(mk2, fileNameSize);

    // create the decrypted file
    const string tmp{(name.begin()), name.end()}; // string tmp(reinterpret_cast<const char*>(name.begin()), name.size());
    const auto originalName = QString::fromStdString(tmp);

    QFile des_file(Utils::uniqueFileName(absolutePath + "/" + originalName));

    if (!des_file.open(QIODevice::WriteOnly))
        return (DES_CANNOT_OPEN_WRITE);

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    auto processed  = 0.;
    auto bytes_read = 0;
    SecureVector<quint8> inBuf(m_const->IN_BUFFER_SIZE + m_const->MACBYTES * 3);
    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), m_const->IN_BUFFER_SIZE + m_const->MACBYTES * 3)) > 0) {
        // calculate percentage proccessed
        processed += bytes_read - m_const->MACBYTES * 3;
        emit updateProgress(src_path, (processed / originalfileSize) * 100);
        try {
            inBuf.resize(bytes_read);
            decrypt.finish(inBuf);
            des_stream.writeRawData(reinterpret_cast<char*>(inBuf.data()), inBuf.size());
            inBuf.resize(m_const->IN_BUFFER_SIZE + m_const->MACBYTES * 3);
        }
        catch (const Botan::Exception&) {
            des_file.remove();
            return (DECRYPT_FAIL);
        }

        if (m_aborted) {
            emit updateProgress(src_info.filePath(), 0);
            des_file.close();
            des_file.remove();
            return (ABORTED_BY_USER);
        }
    }

    if (m_deletefile) {
        src_file.close();
        src_file.remove();
        emit deletedAfterSuccess(src_path);
    }

    return (DECRYPT_SUCCESS);
}

quint32 Crypto_Thread::simpleEncrypt(const QString& src_path)
{

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
    const auto fileSize     = src_info.size();
    const auto fileName     = src_info.fileName().toUtf8();
    const auto fileNameSize = fileName.size();

    AutoSeeded_RNG rng;
    Botan::SecureVector<quint8> argonSalt   = rng.random_vec(m_const->ARGON_SALT_LEN);
    Botan::SecureVector<quint8> tripleNonce = rng.random_vec(m_const->CIPHER_IV_LEN);

    const auto randomData = rng.random_vec(m_const->IN_BUFFER_SIZE);

    // Append the file name to the buffer and some random data
    SecureVector<quint8> master_buffer(fileName.size() + randomData.size());
    memcpy(master_buffer.data(), fileName.data(), fileNameSize);
    memcpy(master_buffer.data() + fileNameSize, randomData.data(), randomData.size());

    // encryption of the buffer who contain the original name of the file
    // and some random data

    SimpleCryptoEngine encrypt(m_algo,true);
    encrypt.setSalt(argonSalt);
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    encrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    encrypt.setNonce(tripleNonce);
    encrypt.finish(master_buffer);

    if (!src_file.exists() || !src_info.isFile())
        return (SRC_CANNOT_OPEN_READ);
    if (!src_file.open(QIODevice::ReadOnly))
        return (SRC_CANNOT_OPEN_READ);

    // create the new file.
    QFile des_file(Utils::uniqueFileName(src_path + m_const->DEFAULT_EXTENSION));
    //QFile des_file(src_path + m_const->DEFAULT_EXTENSION);

    if (des_file.exists())
        return (DES_FILE_EXISTS);
    if (!des_file.open(QIODevice::WriteOnly))
        return (DES_CANNOT_OPEN_WRITE);

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // Write a "magic number" , arsenic version, argon2 parameters, etc...
    des_stream << static_cast<quint32>(m_const->MAGIC_NUMBER);
    des_stream << static_cast<QVersionNumber>(m_const->APP_VERSION);
    des_stream << static_cast<quint32>(m_argonmem);
    des_stream << static_cast<quint32>(m_argoniter);
    des_stream << static_cast<QString>(m_algo);
    des_stream << static_cast<qint64>(fileNameSize);
    des_stream << static_cast<qint64>(fileSize);

    // Write the salt, the 3 nonces and the encrypted header in the file
    des_stream.writeRawData(reinterpret_cast<char*>(argonSalt.data()), m_const->ARGON_SALT_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(tripleNonce.data()), m_const->CIPHER_IV_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // now, move on to the actual data
    QDataStream src_stream(&src_file);
    auto processed  = 0.;
    auto bytes_read = 0;
    SecureVector<quint8> inBuf(m_const->IN_BUFFER_SIZE);

    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), m_const->IN_BUFFER_SIZE)) > 0) {
        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_info.filePath(), (processed / fileSize) * 100);
        // ...
        inBuf.resize(bytes_read);
        encrypt.finish(inBuf);
        des_stream.writeRawData(reinterpret_cast<char*>(inBuf.data()), inBuf.size());
        inBuf.resize(m_const->IN_BUFFER_SIZE);
    }

    if (m_aborted) {
        emit updateProgress(src_info.filePath(), 0);
        des_file.close();
        des_file.remove();
        return (ABORTED_BY_USER);
    }

    if (m_deletefile) {
        src_file.close();
        QFile::remove(src_file.fileName());
        emit deletedAfterSuccess(src_path);
    }
    emit addEncrypted(des_file.fileName());
    return (CRYPT_SUCCESS);
}

quint32 Crypto_Thread::simpleDecrypt(const QString& src_path)
{
    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);
    QString absolutePath = src_info.absolutePath();

    if (!src_file.exists() || !src_info.isFile())
        return (SRC_CANNOT_OPEN_READ);

    if (!src_file.open(QIODevice::ReadOnly))
        return (SRC_CANNOT_OPEN_READ);

    // open the source file and extract all informations necessary for decryption
    QDataStream src_stream(&src_file);
    src_stream.setVersion(QDataStream::Qt_5_0);

    // Read and check the header
    quint32 magic;
    src_stream >> magic;

    if (magic != 0x41525345)
        return (NOT_AN_ARSENIC_FILE);

    // Read and check the version
    QVersionNumber version;
    src_stream >> version;
    emit statusMessage("this file is encrypted with Arsenic version " + version.toString());

    if (version < m_const->APP_VERSION) {
        emit statusMessage("Warning: this is file is encrypted by an old Arsenic Version...");
        emit statusMessage("Warning: version of encrypted file " + version.toString());
        emit statusMessage("Warning: version of your Arsenic " + m_const->APP_VERSION.toString());
    }

    if (version > m_const->APP_VERSION) {
        emit statusMessage("Warning: this file is encrypted with a more recent version of Arsenic...");
        emit statusMessage("Warning: version of encrypted file " + version.toString());
        emit statusMessage("Warning: version of your Arsenic " + m_const->APP_VERSION.toString());
    }

    // Read Argon2 parameters
    quint32 memlimit;
    src_stream >> memlimit;

    quint32 iterations;
    src_stream >> iterations;

    QString algo;
    src_stream >> algo;

    qint64 fileNameSize;
    src_stream >> fileNameSize;

    // On most systems the maximum filename length is 255 bytes
    if (fileNameSize > 255)
        return (SRC_HEADER_READ_ERROR);

    qint64 originalfileSize;
    src_stream >> originalfileSize;

    SecureVector<quint8> salt_buffer(m_const->ARGON_SALT_LEN);
    SecureVector<quint8> tripleNonce(m_const->CIPHER_IV_LEN);
    SecureVector<quint8> master_buffer(fileNameSize + m_const->IN_BUFFER_SIZE + m_const->MACBYTES);

    // Read the salt, the three nonces and the header
    if (!src_stream.readRawData(reinterpret_cast<char*>(salt_buffer.data()), m_const->ARGON_SALT_LEN))
        return (SRC_HEADER_READ_ERROR);

    if (!src_stream.readRawData(reinterpret_cast<char*>(tripleNonce.data()), m_const->CIPHER_IV_LEN))
        return (SRC_HEADER_READ_ERROR);

    if (!src_stream.readRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size()))
        return (SRC_HEADER_READ_ERROR);

    // calculate the internal key with Argon2 and split them in three
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    // decrypt header
    SimpleCryptoEngine decrypt(algo,false);
    decrypt.setSalt(salt_buffer);
    decrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    decrypt.setNonce(tripleNonce);
    try {
        decrypt.finish(master_buffer);
    }
    catch (const Botan::Exception&) {
        return (DECRYPT_FAIL);
    }

    // get from the decrypted header the three internal keys and the original filename
    const quint8* mk2 = &master_buffer[0];
    const OctetString name(mk2, fileNameSize);

    // create the decrypted file
    const string tmp{(name.begin()), name.end()}; // string tmp(reinterpret_cast<const char*>(name.begin()), name.size());
    const auto originalName = QString::fromStdString(tmp);

    QFile des_file(Utils::uniqueFileName(absolutePath + "/" + originalName));

    if (!des_file.open(QIODevice::WriteOnly))
        return (DES_CANNOT_OPEN_WRITE);

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    auto processed  = 0.;
    auto bytes_read = 0;
    SecureVector<quint8> inBuf(m_const->IN_BUFFER_SIZE + m_const->MACBYTES);
    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), m_const->IN_BUFFER_SIZE + m_const->MACBYTES)) > 0) {
        // calculate percentage proccessed
        processed += bytes_read - m_const->MACBYTES;
        emit updateProgress(src_path, (processed / originalfileSize) * 100);
        try {
            inBuf.resize(bytes_read);
            decrypt.finish(inBuf);
            des_stream.writeRawData(reinterpret_cast<char*>(inBuf.data()), inBuf.size());
            inBuf.resize(m_const->IN_BUFFER_SIZE + m_const->MACBYTES);
        }
        catch (const Botan::Exception&) {
            des_file.remove();
            return (DECRYPT_FAIL);
        }
        if (m_aborted) {
            emit updateProgress(src_info.filePath(), 0);
            des_file.close();
            des_file.remove();
            return (ABORTED_BY_USER);
        }
    }

    if (m_deletefile) {
        src_file.close();
        src_file.remove();
        emit deletedAfterSuccess(src_path);
    }

    return (DECRYPT_SUCCESS);
}

void Crypto_Thread::abort()
{
    m_aborted = true;
}
