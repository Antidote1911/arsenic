#include "decrypt.h"
#include "divers.h"
#include "constants.h"
#include "JlCompress.h"
#include <QStringList>
#include <QDir>
#include <QDataStream>
#include <QDebug>
#include "progressbar.h"

#include "botan_all.h"


using namespace ARs;

QString decrypt(QString file, QString pass)
{
    QString result;
    QString decrypt_name;
    // remove any junk in the temp directory
    QString temp_path = QDir::cleanPath(QDir::currentPath() + "/temp");
    clearDir(temp_path);

    result = myDecryptFile(temp_path, file, pass, &decrypt_name);
    QString decrypt_path = QDir::cleanPath(QDir::currentPath() + "/temp/" + decrypt_name);
    QString unzip_name = decrypt_name;
    unzip_name.chop(sizeof(".zip") - 1);
    QString unzip_path = QDir::cleanPath(file + "/" + unzip_name);

    // check if there was an error. if so, do nothing else
    if(result != ("DECRYPT_SUCCESS"))
        return result;
    // otherwise, decryption was successful. unzip the file into the encrypted file's directory
    else
    {

        QuaZip qz(decrypt_path);

        if(QFileInfo::exists(unzip_path))
        {
            return ("ZIP_ERROR");
        }

        else if(qz.open(QuaZip::mdUnzip))
        {
            // check what item type the zipped file is from the embedded comment. depending on the
            // type, change the directory to extract the zip to

            QString item_type = qz.getComment();
            QString unzip_dir;

            QFileInfo source(file);
            if(item_type == "File")
                unzip_dir = source.absolutePath();
            else
                unzip_dir = (source.absolutePath() + "/" + unzip_name);

            qz.close();

            // unzip to target directory
            cout << endl<< "Decompressing. Please be patient..." << endl;
            if(JlCompress::extractDir(decrypt_path, unzip_dir).size() != 0)
            {
                cout << "Successfully decompressed" << endl;

            }
            // otherwise, there was a problem unzipping the decrypted file
            else
                return ("ZIP_ERROR");
        }
        // could not open the decrypted zip file
        else
            return ("could not open the decrypted zip file for extraction");
    }

    // clean up the decrypted zip file
    QFile::remove(decrypt_path);
    return result;
}

QString myDecryptFile(QString des_path, QString src_path, QString key, QString *decrypt_name)
{
    Botan::SecureVector<quint8> main_buffer(IN_BUFFER_SIZE+MACBYTES);

    Botan::SecureVector<quint8> master_key(ARs::KEYBYTES*3);
    Botan::SecureVector<char> pass_buffer(KEYBYTES);
    Botan::SecureVector<quint8> nonce_buffer(NONCEBYTES);
    Botan::SecureVector<quint8> salt_buffer(SALTBYTES);
    Botan::SecureVector<quint8> qint64_buffer(40);
    qint64 len;

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);

    if(!src_file.exists() || !src_info.isFile())
        return "SRC_NOT_FOUND";

    if (!src_file.open(QIODevice::ReadOnly))
        return "SRC_CANNOT_OPEN_READ";

    // open the source file and extract the initial nonce and password salt
    QDataStream src_stream(&src_file);
    src_stream.setVersion(QDataStream::Qt_5_0);

    // Read and check the header
    quint32 magic;
    src_stream >> magic;
    if (magic != 0x41525345)
    {
        return "NOT_ARSENIC_FILE";
    }

    // Read the version
    QString version;    
    src_stream >> version;
    qDebug() << "Decryption: Version of the Encrypted file:" << version;

    if (version < APP_VERSION)
    {
        qDebug() << "Decryption: Warning, this is file is encrypted by an old Arsenic Version:";
        qDebug() << "Version of encrypted file:" << version;
        qDebug() << "Version of your Arsenic:" << APP_VERSION;
    }

    if (version > APP_VERSION)
    {
        qDebug() << "Decryption: Warning, this file is encrypted with a more recent version of Arsenic:";
        qDebug() << "Version of encrypted file:" << version;
        qDebug() << "Version of your Arsenic:" << APP_VERSION;
    }

    // Read Argon2 parameters
    qint32 memlimit;
    src_stream >> memlimit;

    qint32 iterations;
    src_stream >> iterations;

    // Read crypto algo parameters
    QString cryptoAlgo;
    src_stream >> cryptoAlgo;

    // Read additional data (username)
    QString userName;
    src_stream >> userName;

    std::unique_ptr<Botan::AEAD_Mode> dec = Botan::AEAD_Mode::create(cryptoAlgo.toStdString(), Botan::DECRYPTION);
    std::string add_data(userName.toStdString());
    std::vector<uint8_t> add(add_data.data(),add_data.data()+add_data.length());

    if(src_stream.readRawData(reinterpret_cast<char *>(nonce_buffer.data()), NONCEBYTES)
        < static_cast<int>(NONCEBYTES))
        return "SRC_HEADER_READ_ERROR";

    if(src_stream.readRawData(reinterpret_cast<char *>(salt_buffer.data()),
        SALTBYTES) < static_cast<int>(SALTBYTES))
        return "SRC_HEADER_READ_ERROR";

    // Convert the QString password to securevector
    pass_buffer = convertStringToSecureVector(key);

    // Calculate the encryption key with Argon2
    cout << "Argon2 passphrase derivation... Please wait." << endl;
    master_key = calculateHash(pass_buffer, salt_buffer, memlimit, iterations);
    const uint8_t* mk = master_key.begin().base();
    const Botan::SymmetricKey cipher_key1(mk, KEYBYTES);
    const Botan::SymmetricKey cipher_key2(&mk[KEYBYTES], KEYBYTES);
    const Botan::SymmetricKey cipher_key3(&mk[KEYBYTES+KEYBYTES], KEYBYTES);

    // next, get the encrypted header size and decrypt it
    qint64 header_size;

    src_stream.readRawData(reinterpret_cast<char *>(qint64_buffer.data()), 40);

    dec->set_key(cipher_key1);
    dec->set_ad(add);
    dec->start(nonce_buffer);
    try
    {
    dec->finish(qint64_buffer);
    }
    catch(Botan::Exception& e)
    {
        return e.what();
    }

    header_size = qFromLittleEndian<qint64>(qint64_buffer.data() + MACBYTES);

    src_stream.readRawData(reinterpret_cast<char *>(main_buffer.data()), main_buffer.size());

    // get the file size and filename of original item
    Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
    dec->set_key(cipher_key2);
    dec->start(nonce_buffer);
    try
    {
    dec->finish(main_buffer);
    }
    catch(Botan::Exception& e)
    {
        return e.what();
    }

    qint64 filesize = qFromLittleEndian<qint64>(main_buffer.data() + MACBYTES);
    len = header_size - MACBYTES - sizeof(qint64);

    QString des_name = QString::fromUtf8(reinterpret_cast<char *>(main_buffer.data() +
        MACBYTES + sizeof(qint64)), len);

    // return the name of the decrypted file
    if(decrypt_name != nullptr)
        *decrypt_name = des_name;

    // create the decrypted file in the passed directory
    QFile des_file(QDir::cleanPath(des_path + "/" + des_name));

    if(des_file.exists())
        return "DES_FILE_EXISTS";

    if(!des_file.open(QIODevice::WriteOnly))
        return "DES_CANNOT_OPEN_WRITE";

    QDataStream des_stream(&des_file);

    ///////////////////////
    // start decrypting the actual data
    cout << "Decryption... Please wait." << endl;
    dec->set_key(cipher_key3);
    dec->set_ad(add);

    // increment the nonce, decrypt and write the plaintext block to the destination
    Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
    dec->start(nonce_buffer);
    double processed = 0;
    while(!src_stream.atEnd())
    {
        len = src_stream.readRawData(reinterpret_cast<char *>(main_buffer.data()), IN_BUFFER_SIZE);

        if (!src_stream.atEnd())
        {


            dec->update(main_buffer);
            des_stream.writeRawData(reinterpret_cast<char *>(main_buffer.data()), main_buffer.size());
        }

        if (src_stream.atEnd())
        {

            main_buffer.resize(MACBYTES);
            len = src_stream.readRawData(reinterpret_cast<char *>(main_buffer.data()), main_buffer.size());
            try
            {
            dec->finish(main_buffer);
            }
            catch(Botan::Exception& e)
            {
                return e.what();
            }
        }

            // Calculate progress in percent
            processed += len;
            double p = (processed / filesize) * 100; // calculate percentage proccessed
            printProgress(p / 100); // show updated progress
        }

    return "DECRYPT_SUCCESS";
}
