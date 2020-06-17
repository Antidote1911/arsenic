#include "fileCrypto.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringBuilder>
#include <QThread>

#include "botan_all.h"
#include "constants.h"
#include "messages.h"
#include "tripleencryption.h"
#include "utils.h"
#include "JlCompress.h"

using namespace ARs;
using namespace Botan;
using namespace std;

Crypto_Thread::Crypto_Thread(QObject* parent)
    : QThread(parent)
{
}

void Crypto_Thread::setParam(bool direction,
                             QStringList const& filenames,
                             const QString& password,
                             quint32 argonmem,
                             quint32 argoniter,
                             bool deletefile)
{
    m_filenames  = filenames;
    m_password   = password;
    m_direction  = direction;
    m_deletefile = deletefile;

    if (argonmem == 0)
        m_argonmem = MEMLIMIT_INTERACTIVE;
    if (argonmem == 1)
        m_argonmem = MEMLIMIT_MODERATE;
    if (argonmem == 2)
        m_argonmem = MEMLIMIT_SENSITIVE;

    ////////////////////////////////////////////////

    if (argoniter == 0)
        m_argoniter = ITERATION_INTERACTIVE;

    if (argoniter == 1)
        m_argoniter = ITERATION_MODERATE;

    if (argoniter == 2)
        m_argoniter = ITERATION_SENSITIVE;
}

void Crypto_Thread::run()
{
    for (auto& inputFileName : m_filenames) {
        if (m_aborted) {
            m_aborted = true;
            Crypto_Thread::terminate();
            return;
        }
        // remove any junk in the temp directory
        QString temp_path = Utils::getTempPath();
        Utils::clearDir(temp_path);

        QFile src_file(QDir::cleanPath(inputFileName));
        QFileInfo src_info(src_file);
        QByteArray src_name = src_info.fileName().toUtf8();

        if (m_direction == true) {
            emit statusMessage("");
            emit statusMessage(QDateTime::currentDateTime().toString("dddd dd MMMM yyyy (hh:mm:ss)") + " encryption of " + inputFileName);
            // test if it is a floler. If yes, compress it before encrypt

            QString zip_path = QDir::cleanPath(Utils::getTempPath() + "tmp.zip");
            QuaZip qz(zip_path);

            quint32 result = 0;
            if (src_info.isFile()) {
                int ret_val = JlCompress::compressFile(zip_path, inputFileName);
                if (ret_val) {
                    qz.open(QuaZip::mdAdd);
                    qz.setComment("ARSENIC FILE");
                    qz.close();
                }
                result = encrypt(zip_path, inputFileName);
                QFile::remove(zip_path);
            }
            else if (src_info.isDir()) {
                int ret_val = JlCompress::compressDir(zip_path, inputFileName);
                if (ret_val) {
                    qz.open(QuaZip::mdAdd);
                    qz.setComment("ARSENIC DIR");
                    qz.close();
                }

                result = encrypt(zip_path, inputFileName);

                QFile::remove(zip_path);
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
            QString decrypt_name;
            quint32 result       = decrypt(inputFileName, &decrypt_name);
            QString decrypt_path = Utils::getTempPath() + "tmp.zip";
            QuaZip qz(decrypt_path);
            qz.open(QuaZip::mdUnzip);
            QString item_type = qz.getComment();
            QString unzip_dir;
            if (item_type == "ARSENIC FILE")
                unzip_dir = QDir::cleanPath(src_info.path());
            else
                unzip_dir = QDir::cleanPath(src_info.path() + "/" + decrypt_name);

            qz.close();
            JlCompress::extractDir(decrypt_path, unzip_dir);
            QFile::remove(decrypt_path);

            emit statusMessage(errorCodeToString(result));

            if (m_aborted) {
                m_aborted = false; // Reset abort flag
            }
        }
    }
}

quint32 Crypto_Thread::encrypt(const QString& src_path, const QString& des_path)
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
    QFileInfo src_info2(des_path);
    const auto fileSize     = src_info.size();
    const auto fileName     = src_info2.fileName().toUtf8();
    const auto fileNameSize = fileName.size();

    emit updateProgress(src_info2.filePath(), 0);

    AutoSeeded_RNG rng;
    auto argonSalt        = rng.random_vec(ARGON_SALT_LEN);
    auto tripleNonce      = rng.random_vec(CIPHER_IV_LEN * 3);
    const auto randomData = rng.random_vec(IN_BUFFER_SIZE);

    // Append the file name to the buffer and some random data
    SecureVector<quint8> master_buffer(fileName.size() + randomData.size());
    memcpy(master_buffer.data(), fileName.data(), fileNameSize);
    memcpy(master_buffer.data() + fileNameSize, randomData.data(), randomData.size());

    // Triple encryption of the buffer who contain the original name of the file
    // and some random data

    TripleEncryption encrypt(true);
    encrypt.setSalt(argonSalt);
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    encrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    encrypt.setTripleNonce(tripleNonce);
    encrypt.finish(master_buffer);

    if (!src_file.exists() || !src_info.isFile())
        return (SRC_CANNOT_OPEN_READ);
    if (!src_file.open(QIODevice::ReadOnly))
        return (SRC_CANNOT_OPEN_READ);

    // create the new file.
    QFile des_file(des_path + DEFAULT_EXTENSION);

    if (des_file.exists())
        return (DES_FILE_EXISTS);
    if (!des_file.open(QIODevice::WriteOnly))
        return (DES_CANNOT_OPEN_WRITE);

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
    auto processed  = 0.;
    auto bytes_read = 0;
    SecureVector<quint8> inBuf(IN_BUFFER_SIZE);

    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), IN_BUFFER_SIZE)) > 0) {
        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_info2.filePath(), (processed / fileSize) * 100);
        // ...
        inBuf.resize(bytes_read);
        encrypt.finish(inBuf);
        des_stream.writeRawData(reinterpret_cast<char*>(inBuf.data()), inBuf.size());
        inBuf.resize(IN_BUFFER_SIZE);
    }

    if (m_aborted) {
        emit updateProgress(src_info2.filePath(), 0);
        des_file.close();
        des_file.remove();
        return (ABORTED_BY_USER);
    }

    if (m_deletefile) {
        QFile::remove(des_path);
        emit deletedAfterSuccess(des_path);
    }
    emit addEncrypted(des_file.fileName());
    return (CRYPT_SUCCESS);
}

quint32 Crypto_Thread::decrypt(const QString& src_path, QString* decrypt_name)
{
    QString temp_path = Utils::getTempPath();

    emit updateProgress(src_path, 0);
    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);

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

    if (version < APP_VERSION) {
        emit statusMessage("Warning: this is file is encrypted by an old Arsenic Version...");
        emit statusMessage("Warning: version of encrypted file " + version.toString());
        emit statusMessage("Warning: version of your Arsenic " + APP_VERSION.toString());
    }

    if (version > APP_VERSION) {
        emit statusMessage("Warning: this file is encrypted with a more recent version of Arsenic...");
        emit statusMessage("Warning: version of encrypted file " + version.toString());
        emit statusMessage("Warning: version of your Arsenic " + APP_VERSION.toString());
    }

    // Read Argon2 parameters
    quint32 memlimit;
    src_stream >> memlimit;

    quint32 iterations;
    src_stream >> iterations;

    qint64 fileNameSize;
    src_stream >> fileNameSize;

    // On most systems the maximum filename length is 255 bytes
    if (fileNameSize > 255)
        return (SRC_HEADER_READ_ERROR);

    qint64 originalfileSize;
    src_stream >> originalfileSize;

    SecureVector<quint8> salt_buffer(ARGON_SALT_LEN);
    SecureVector<quint8> tripleNonce(CIPHER_IV_LEN * 3);
    SecureVector<quint8> master_buffer(fileNameSize + IN_BUFFER_SIZE + MACBYTES * 3);

    // Read the salt, the three nonces and the header
    if (!src_stream.readRawData(reinterpret_cast<char*>(salt_buffer.data()), ARGON_SALT_LEN))
        return (SRC_HEADER_READ_ERROR);

    if (!src_stream.readRawData(reinterpret_cast<char*>(tripleNonce.data()), CIPHER_IV_LEN * 3))
        return (SRC_HEADER_READ_ERROR);

    if (!src_stream.readRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size()))
        return (SRC_HEADER_READ_ERROR);

    // calculate the internal key with Argon2 and split them in three
    emit statusMessage("Argon2 passphrase derivation... Please wait.");

    // decrypt header
    TripleEncryption decrypt(false);
    decrypt.setSalt(salt_buffer);
    decrypt.derivePassword(m_password, m_argonmem, m_argoniter);
    decrypt.setTripleNonce(tripleNonce);
    try {
        decrypt.finish(master_buffer);
    }
    catch (const Botan::Exception&) {
        return (DECRYPT_FAIL);
    }

    // get from the decrypted header the three internal keys and the original filename
    const quint8* mk2 = master_buffer.begin().base();
    const OctetString name(mk2, fileNameSize);

    // create the decrypted file
    const string tmp{(name.begin()), name.end()}; // string tmp(reinterpret_cast<const char*>(name.begin()), name.size());
    const auto originalName = QString::fromStdString(tmp);

    // return the name of the decrypted file
    if (decrypt_name != nullptr)
        *decrypt_name = originalName;

    QFile des_file(temp_path + "/tmp.zip");

    if (!des_file.open(QIODevice::WriteOnly))
        return (DES_CANNOT_OPEN_WRITE);

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // decrypt the file and write the plaintext to the destination.
    emit statusMessage("Decryption... Please wait.");

    auto processed  = 0.;
    auto bytes_read = 0;
    SecureVector<quint8> inBuf(IN_BUFFER_SIZE + MACBYTES * 3);
    while (!m_aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(inBuf.data()), IN_BUFFER_SIZE + MACBYTES * 3)) > 0) {
        // calculate percentage proccessed
        processed += bytes_read - MACBYTES * 3;
        emit updateProgress(src_path, (processed / originalfileSize) * 100);
        try {
            inBuf.resize(bytes_read);
            decrypt.finish(inBuf);
            des_stream.writeRawData(reinterpret_cast<char*>(inBuf.data()), inBuf.size());
            inBuf.resize(IN_BUFFER_SIZE + MACBYTES * 3);
        }
        catch (const Botan::Exception&) {
            des_file.remove();
            return (DECRYPT_FAIL);
        }
        emit updateProgress(src_path, 100);
    }

    if (m_deletefile) {
        src_file.close();
        src_file.remove();
        emit deletedAfterSuccess(src_path);
    }
    QFileInfo des_info(des_file);
    return (DECRYPT_SUCCESS);
}

QString Crypto_Thread::uniqueFileName(const QString& fileName)
{
    QFileInfo originalFile(fileName);
    auto uniqueFileName      = fileName;
    auto foundUniqueFileName = false;
    auto i                   = 0;
    while (!foundUniqueFileName && i < 100000) {
        QFileInfo uniqueFile(uniqueFileName);
        if (uniqueFile.exists() && uniqueFile.isFile()) // Write number of copies before file extension
        {
            uniqueFileName = originalFile.absolutePath() % QDir::separator() % originalFile.baseName() % QString{" (%1)"}.arg(i + 2);

            if (!originalFile.completeSuffix().isEmpty()) // Add the file extension if there is one
            {
                uniqueFileName += QStringLiteral(".") % originalFile.completeSuffix();
            }
            ++i;
        }
        else {
            foundUniqueFileName = true;
        }
    }
    return (uniqueFileName);
}

void Crypto_Thread::abort()
{
    m_aborted = true;
}
