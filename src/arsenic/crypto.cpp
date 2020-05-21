#include "crypto.h"

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
        if (aborted) {
            aborted = true;
            Crypto_Thread::terminate();
            return;
        }

        if (m_direction == true) {
            try {
                emit statusMessage("");
                emit statusMessage(QDateTime::currentDateTime().toString(
                                       "dddd dd MMMM yyyy (hh:mm:ss)")
                    + " encryption of " + inputFileName);

                encrypt(inputFileName);
                if (aborted) // Reset abort flag
                {
                    aborted = false;
                    return;
                }
            } catch (const Botan::Stream_IO_Error&) {
                emit statusMessage("Error: Botan::Stream_IO_Error");
            } catch (const std::exception& e) {
                const auto error = QString { e.what() };
                emit statusMessage(error);
            }
        } else {
            try {
                emit statusMessage("");
                emit statusMessage(QDateTime::currentDateTime().toString(
                                       "dddd dd MMMM yyyy (hh:mm:ss)")
                    + " decryption of " + inputFileName);
                decrypt(inputFileName);
                if (aborted) // Reset abort flag

                    aborted = false;
            } catch (const Botan::Decoding_Error&) {
                emit statusMessage("Error: Can't decrypt file. Wrong password entered "
                                   "or the file has been corrupted.");
                QFile::remove(outfileresult);
            } catch (const Botan::Integrity_Failure&) {
                emit statusMessage("Error: Can't decrypt file. Wrong password.");
                QFile::remove(outfileresult);
            } catch (const Botan::Invalid_Argument&) {
                emit statusMessage(
                    "Error: Can't decrypt file. Is it an encrypted file ?");
                QFile::remove(outfileresult);
            } catch (const std::invalid_argument&) {
                emit statusMessage(
                    "Error: Can't decrypt file. Is it an encrypted file ?");
                QFile::remove(outfileresult);
            } catch (const std::runtime_error&) {
                emit statusMessage("Error: Can't read file.");
            } catch (const std::exception& e) {
                const auto error = QString(e.what());
                emit statusMessage("Error: " + error);
            }
        }
    }
}

void Crypto_Thread::encrypt(const QString src_path)
{
    emit updateProgress(src_path, 0);
    /* format is:
    MAGIC_NUMBER
    APP_VERSION
    Argon memlimit
    Argon iterations
    original fileName
    original fileSize
    Argon salt  (16 bytes)
    ivChaCha20  (24 bytes)
    ivAES       (24 bytes)
    ivSerpent   (24 bytes)
    encrypted internal_master_key  (32 bytes* 3 + MACBYTES * 3)
    encrypted file


    */

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);
    const auto filesize { src_info.size() };
    const auto fileName { src_info.fileName().toUtf8() };
    const auto fileNameSize { fileName.size() };
    const auto filename { fileName.toStdString() };

    SecureVector<quint8> fileN(filename.begin(), filename.end());
    SecureVector<quint8> master_buffer(CIPHER_KEY_LEN * 3 + fileNameSize);

    // Convert the QString password to securevector
    auto pass { m_password.toStdString() };
    SecureVector<char> password(pass.begin(), pass.end());

    auto add_data { (APP_URL.toStdString()) };
    SecureVector<quint8> add(add_data.data(), add_data.data() + add_data.length());

    // Randomize the 16 bytes salt and the three 24 bytes nonces
    AutoSeeded_RNG rng;
    auto argonSalt { rng.random_vec(ARGON_SALT_LEN) };

    auto ivChaCha20 { rng.random_vec(CIPHER_IV_LEN) };
    auto ivAES { rng.random_vec(CIPHER_IV_LEN) };
    auto ivSerpent { rng.random_vec(CIPHER_IV_LEN) };

    // Append the file name to the buffer and the 3 Random key, and split them in 3 for file encryption
    memcpy(master_buffer.data(), fileN.data(), fileN.size());
    rng.randomize(master_buffer.data() + fileNameSize, CIPHER_KEY_LEN * 3);

    const quint8* mk { master_buffer.begin().base() };
    const OctetString name(mk, fileNameSize);
    const SymmetricKey cipherkey1(&mk[fileNameSize], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey2(&mk[fileNameSize + CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey3(&mk[fileNameSize + CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    // chained triple encryption of the internal_master_key with the random 3 master_key
    emit statusMessage("Argon2 passphrase derivation... Please wait.");
    const auto master_key { calculateHash(password, argonSalt, m_argonmem, m_argoniter) };
    const quint8* mk2 { master_key.begin().base() };
    const SymmetricKey ChaCha20_key(mk2, CIPHER_KEY_LEN);
    const SymmetricKey AES_key(&mk2[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey Serpent_key(&mk2[CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    const unique_ptr<AEAD_Mode> master1 = AEAD_Mode::create("ChaCha20Poly1305", ENCRYPTION);
    const unique_ptr<AEAD_Mode> master2 = AEAD_Mode::create("AES-256/EAX", ENCRYPTION);
    const unique_ptr<AEAD_Mode> master3 = AEAD_Mode::create("Serpent/GCM", ENCRYPTION);

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
        emit statusMessage("Error: file exist ?");
        return;
    }
    if (!src_file.open(QIODevice::ReadOnly)) {
        emit statusMessage("Error: can't read file");
        return;
    }

    // create the new file, the path should normally be to the same directory as
    // the source

    QFile des_file(QDir::cleanPath(src_path) + DEFAULT_EXTENSION);

    if (des_file.exists()) {
        emit statusMessage("Error: the output file already exist");
        return;
    }
    if (!des_file.open(QIODevice::WriteOnly)) {
        emit statusMessage("Error: the output file can't be open for writing");
        return;
    }

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // Write a header with a "magic number" , arsenic version, argon2 parameters,
    // crypto algorithm
    des_stream << static_cast<quint32>(MAGIC_NUMBER);
    des_stream << static_cast<QString>(APP_VERSION);
    des_stream << static_cast<quint32>(m_argonmem);
    des_stream << static_cast<quint32>(m_argoniter);
    des_stream << static_cast<quint32>(filesize);
    des_stream << static_cast<quint32>(fileNameSize);

    // Write the salt in file
    des_stream.writeRawData(reinterpret_cast<char*>(argonSalt.data()), ARGON_SALT_LEN);

    // Write the 3 nonces in the file
    des_stream.writeRawData(reinterpret_cast<char*>(ivChaCha20.data()), CIPHER_IV_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(ivAES.data()), CIPHER_IV_LEN);
    des_stream.writeRawData(reinterpret_cast<char*>(ivSerpent.data()), CIPHER_IV_LEN);
    // Write the buffer with the 3 encrypted internal keys and the encrypted filename in the file
    des_stream.writeRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // now, move on to the actual data
    QDataStream src_stream(&src_file);

    ///////////////////////
    emit statusMessage("Encryption... Please wait...");

    const unique_ptr<AEAD_Mode> enc1 = AEAD_Mode::create("ChaCha20Poly1305", ENCRYPTION);
    const unique_ptr<AEAD_Mode> enc2 = AEAD_Mode::create("AES-256/EAX", ENCRYPTION);
    const unique_ptr<AEAD_Mode> enc3 = AEAD_Mode::create("Serpent/GCM", ENCRYPTION);

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

    double processed { 0 };
    quint32 bytes_read;
    SecureVector<quint8> buf(IN_BUFFER_SIZE);
    while (!aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(buf.data()), IN_BUFFER_SIZE)) > 0) {

        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_path, (processed / filesize) * 100);

        if (bytes_read < IN_BUFFER_SIZE) {
            buf.resize(bytes_read);
            enc1->finish(buf);
            enc2->finish(buf);
            enc3->finish(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), buf.size());
        } else {
            enc1->update(buf); // 65536
            enc2->update(buf);
            enc3->update(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), bytes_read);
        }
    }

    if (aborted) {
        emit statusMessage("Encryption aborted by user. The incomplete encrypted "
                           "file was deleted.");
        emit updateProgress(src_path, 0);
        des_file.remove();
        return;
    }

    emit statusMessage("Encryption finished.");
    QFileInfo fileInfo(des_file.fileName());

    if (m_deletefile == true) {
        src_file.close();
        QFile::remove(src_path);
        emit sourceDeletedAfterSuccess(src_path);
    }

    emit addEncrypted(fileInfo.filePath());
}

void Crypto_Thread::decrypt(QString src_path)
{

    emit updateProgress(src_path, 0);

    // Convert the QString password to securevector
    const string pass { m_password.toStdString() };
    const SecureVector<char> password(pass.begin(), pass.end());

    SecureVector<quint8> master_key(CIPHER_KEY_LEN * 3);
    SecureVector<quint8> salt_buffer(ARGON_SALT_LEN);
    SecureVector<quint8> ivChaCha20(CIPHER_IV_LEN);
    SecureVector<quint8> ivAES(CIPHER_IV_LEN);
    SecureVector<quint8> ivSerpent(CIPHER_IV_LEN);

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);
    const auto src_size { src_info.size() };

    if (!src_file.exists() || !src_info.isFile()) {
        emit statusMessage("Error: can't read the file");
        return;
    }

    if (!src_file.open(QIODevice::ReadOnly)) {
        emit statusMessage("Error: can't read the file");
        return;
    }

    // open the source file and extract the initial nonce and password salt
    QDataStream src_stream(&src_file);
    src_stream.setVersion(QDataStream::Qt_5_0);

    // Read and check the header
    quint32 magic;
    src_stream >> magic;
    if (magic != 0x41525345) {
        emit statusMessage("Error: this file is not an Arsenic file");
        return;
    }

    // Read the version
    QString version;
    src_stream >> version;
    emit statusMessage("this file is encrypted with Arsenic version :" + version);

    if (version < APP_VERSION) {
        emit statusMessage(
            "Warning: this is file is encrypted by an old Arsenic Version...");
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

    // Read original filezize
    quint32 originalFileSize;
    src_stream >> originalFileSize;

    // Read original fileNameSize
    quint32 fileNameSize;
    src_stream >> fileNameSize;

    std::string add_data(APP_URL.toStdString());
    SecureVector<quint8> add(add_data.data(), add_data.data() + add_data.length());

    SecureVector<quint8> master_buffer(fileNameSize + CIPHER_KEY_LEN * 3 + MACBYTES * 3);

    src_stream.readRawData(reinterpret_cast<char*>(salt_buffer.data()), ARGON_SALT_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(ivChaCha20.data()), CIPHER_IV_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(ivAES.data()), CIPHER_IV_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(ivSerpent.data()), CIPHER_IV_LEN);
    src_stream.readRawData(reinterpret_cast<char*>(master_buffer.data()), master_buffer.size());

    // decrypt internal keys and original filename
    emit statusMessage("Argon2 passphrase derivation... Please wait.");
    master_key = calculateHash(password, salt_buffer, memlimit, iterations);

    const quint8* mk = master_key.begin().base();
    const SymmetricKey ChaCha20_key(mk, CIPHER_KEY_LEN);
    const SymmetricKey AES_key(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey Serpent_key(&mk[CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    const unique_ptr<AEAD_Mode> master1 = AEAD_Mode::create("Serpent/GCM", DECRYPTION);
    const unique_ptr<AEAD_Mode> master2 = AEAD_Mode::create("AES-256/EAX", DECRYPTION);
    const unique_ptr<AEAD_Mode> master3 = AEAD_Mode::create("ChaCha20Poly1305", DECRYPTION);

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

    const quint8* mk2 = master_buffer.begin().base();
    const OctetString name(mk2, fileNameSize);
    const SymmetricKey cipherkey1(&mk2[fileNameSize], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey2(&mk2[fileNameSize + CIPHER_KEY_LEN], CIPHER_KEY_LEN);
    const SymmetricKey cipherkey3(&mk2[fileNameSize + CIPHER_KEY_LEN + CIPHER_KEY_LEN], CIPHER_KEY_LEN);

    // create the decrypted file
    const string tmp { (name.begin()), name.end() };
    const QString originalName { QString::fromStdString(tmp) };
    QFile des_file(QDir::cleanPath(src_info.absolutePath() + "/" + originalName));

    if (des_file.exists()) {
        emit statusMessage("Error: the output file already exist");
        return;
    }

    if (!des_file.open(QIODevice::WriteOnly)) {
        emit statusMessage("Error: the output file can't be open for writing");
        return;
    }

    QDataStream des_stream(&des_file);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // start decrypting the actual data
    emit statusMessage("Decryption... Please wait.");

    // increment the nonce, decrypt and write the plaintext block to the
    // destination

    const unique_ptr<AEAD_Mode> decSerpent = AEAD_Mode::create("Serpent/GCM", DECRYPTION);
    const unique_ptr<AEAD_Mode> decAES = AEAD_Mode::create("AES-256/EAX", DECRYPTION);
    const unique_ptr<AEAD_Mode> decChaCha = AEAD_Mode::create("ChaCha20Poly1305", DECRYPTION);

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

    double processed { 0 };
    quint32 bytes_read;
    SecureVector<quint8> buf(IN_BUFFER_SIZE);
    while (!aborted && (bytes_read = src_stream.readRawData(reinterpret_cast<char*>(buf.data()), IN_BUFFER_SIZE)) > 0) {

        // calculate percentage proccessed
        processed += bytes_read;
        emit updateProgress(src_path, (processed / src_size) * 100);

        if (bytes_read < IN_BUFFER_SIZE) {
            buf.resize(bytes_read);
            decSerpent->finish(buf);
            decAES->finish(buf);
            decChaCha->finish(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), buf.size());
        } else {
            decSerpent->update(buf); // 65536
            decAES->update(buf);
            decChaCha->update(buf);
            des_stream.writeRawData(reinterpret_cast<char*>(buf.data()), bytes_read);
        }
    }

    if (m_deletefile == true) {
        src_file.close();
        QFile::remove(src_path);
        emit sourceDeletedAfterSuccess(src_path);
    }

    QFileInfo des_info(des_file);
    emit addDecrypted(des_info.filePath());
    emit statusMessage("Decryption finished.");
}

Botan::SecureVector<char>
Crypto_Thread::convertStringToSecureVector(QString password)
{
    Botan::SecureVector<char> pass_buffer(CIPHER_KEY_LEN);

    memset(pass_buffer.data(), 0, CIPHER_KEY_LEN);
    memcpy(pass_buffer.data(),
        password.toUtf8().constData(),
        std::min(password.toUtf8().size(), static_cast<int>(CIPHER_KEY_LEN)));
    return pass_buffer;
}

SecureVector<quint8> Crypto_Thread::calculateHash(SecureVector<char> pass_buffer,
    SecureVector<quint8> salt_buffer,
    quint32 memlimit,
    quint32 iterations)
{
    auto pwdhash_fam = PasswordHashFamily::create("Argon2id");
    SecureVector<quint8> key_buffer(CIPHER_KEY_LEN * 3);

    // mem,ops,threads
    auto default_pwhash = pwdhash_fam->from_params(memlimit, iterations, PARALLELISM_INTERACTIVE);

    default_pwhash->derive_key(key_buffer.data(),
        key_buffer.size(),
        pass_buffer.data(),
        pass_buffer.size(),
        salt_buffer.data(),
        salt_buffer.size());
    return key_buffer;
}

QString
Crypto_Thread::removeExtension(const QString& fileName,
    const QString& extension)
{
    QFileInfo file { fileName };
    QString newFileName = fileName;

    if (file.suffix() == extension)
        newFileName = file.absolutePath() % QDir::separator() % file.completeBaseName();

    return newFileName;
}

QString
Crypto_Thread::uniqueFileName(const QString& fileName)
{
    QFileInfo originalFile { fileName };
    QString uniqueFileName = fileName;

    auto foundUniqueFileName = false;
    auto i = 0;

    while (!foundUniqueFileName && i < 100000) {
        QFileInfo uniqueFile { uniqueFileName };

        if (uniqueFile.exists() && uniqueFile.isFile()) // Write number of copies before file extension
        {
            uniqueFileName = originalFile.absolutePath() % QDir::separator() % originalFile.baseName() % QString { " (%1)" }.arg(i + 2);

            if (!originalFile.completeSuffix()
                     .isEmpty()) // Add the file extension if there is one

                uniqueFileName += QStringLiteral(".") % originalFile.completeSuffix();

            ++i;
        } else {
            foundUniqueFileName = true;
        }
    }
    return uniqueFileName;
}
