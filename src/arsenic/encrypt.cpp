#include "encrypt.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include "JlCompress.h"
#include "divers.h"
#include <QFileInfo>
#include <QDebug>
#include <QDataStream>
#include "constants.h"
#include "progressbar.h"
#include <QStorageInfo>

#include "botan_all.h"

#include <QtEndian>

using namespace ARs;

QString encrypt(QString file, QString pass)
{


    QString result;
    // remove any junk in the temp directory
    QString temp_path = QDir::cleanPath(QDir::currentPath() + "/temp");
    clearDir(temp_path);

    QFileInfo source(file);

    bool ret_val = false;
    QString zip_path = QDir::cleanPath(QDir::currentPath() + "/temp/" + source.fileName() + ".zip");

    bool isEmptyFolder;
    isEmptyFolder = QDir(file).entryList(QDir::NoDotAndDotDot|QDir::AllEntries).count() == 0;

    QuaZip qz(zip_path);

    // try and compress the file or directory

    if(source.isFile())
    {

        if (source.size()==0)
        {
            return "Empty File";
        }
        cout << "Compressing the file..." << endl;
        ret_val = JlCompress::compressFile(zip_path, file);

        // if the zip file was created, add a comment on what type it originally was
        if(ret_val)
        {
            qz.open(QuaZip::mdAdd);
            qz.setComment("File");
            qz.close();
            cout << "The file was compressed succesfully" << endl;
        }
    }
    else
    {
        qint64 size = dirSize(file);
        qInfo() << "folder size is: " + formatSize(size);

        if (isEmptyFolder==true || size==0)
        {
            return "Empty Folder";
        }

        cout << "Compressing the folder..." << endl;
        ret_val = JlCompress::compressDir(zip_path, file);

        if(ret_val)
        {
            qz.open(QuaZip::mdAdd);
            qz.setComment("Directory");
            qz.close();
            cout << "The folder was compressed succesfully" << endl;
        }

    }

    // if successful, try encrypting the zipped file
    if(ret_val)
    {
        // create the base name of the final encrypted file. defaults to the original filename
        // unless the user specified something for encrypt_name
        QString encrypt_path;
        encrypt_path = QDir::cleanPath(file +"." +ARs::DEFAULT_EXTENSION);
        result = myEncryptFile(zip_path, encrypt_path, pass);
        // delete the intermediate zip file when done
            QFile::remove(zip_path);
    }
        // could not create the intermediate zip file
    else
    {
        return "ZIP_ERROR";
    }

return (result);
}

QString myEncryptFile(QString src_path, QString encrypt_path, QString key)
{

    Botan::SecureVector<quint8> main_buffer(IN_BUFFER_SIZE);
    Botan::SecureVector<char> pass_buffer(ARs::KEYBYTES);
    Botan::SecureVector<quint8> master_key(ARs::KEYBYTES);     // 256 bits (32 bytes)
    Botan::SecureVector<quint8> nonce_buffer(ARs::NONCEBYTES); // 192 bits (24 bytes)
    Botan::SecureVector<quint8> salt_buffer(ARs::SALTBYTES);
    Botan::SecureVector<quint8> qint64_buffer(ARs::MACBYTES + sizeof(qint64));
    qint64 len;

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);

    if(!src_file.exists() || !src_info.isFile())
        return ("SRC_NOT_FOUND");

    if (!src_file.open(QIODevice::ReadOnly))
        return ("SRC_CANNOT_OPEN_READ");

    // Convert the QString password to securevector
    pass_buffer = convertStringToSecureVector(key);

    // Randomize the 16 bytes salt
    Botan::AutoSeeded_RNG rng;
    salt_buffer = rng.random_vec(SALTBYTES);

    // Calculate the encryption key with Argon2
    cout << "Generating key by Argon2. Please be patient..." << endl;
    master_key = calculateHash(pass_buffer, salt_buffer, MEMLIMIT_SENSITIVE, ITERATION_SENSITIVE);
    cout << "Key generated successfully" << endl;

    // create the new file, the path should normally be to the same directory as the source
    QFile des_file(encrypt_path);

    if(des_file.exists())
        return ("DES_FILE_EXISTS");

    if(!des_file.open(QIODevice::WriteOnly))
        return ("DES_CANNOT_OPEN_WRITE");

    QDataStream des_stream(&des_file);

    // Write a header with a "magic number" and a version
    des_stream << static_cast<quint32> (ARs::MAGIC_NUMBER);
    des_stream << static_cast<QString> (ARs::APP_VERSION);
    des_stream << static_cast<qint32>(MEMLIMIT_SENSITIVE);
    des_stream << static_cast<qint32>(ITERATION_SENSITIVE);
    des_stream.setVersion(QDataStream::Qt_5_12);

    // Generate a 24 bytes random initial nonce
    nonce_buffer = rng.random_vec(NONCEBYTES);

    // Write the initial nonce in file
    if(des_stream.writeRawData(reinterpret_cast<char *>(nonce_buffer.data()), NONCEBYTES)
        < static_cast<int>(NONCEBYTES))
        return "DES_HEADER_WRITE_ERROR";

    // Write the salt in file
    if(des_stream.writeRawData(reinterpret_cast<char *>(salt_buffer.data()),
        SALTBYTES) < static_cast<int>(SALTBYTES))
        return "DES_HEADER_WRITE_ERROR";

    // move the file size and name data into the main buffer
    QByteArray src_name = src_info.fileName().toUtf8();

    qint64 filesize = src_info.size();
    qToLittleEndian<qint64>(filesize, main_buffer.data() + MACBYTES);

    len = std::min(static_cast<long unsigned int>(src_name.size()), BUFFER_SIZE_WITHOUT_MAC -
        sizeof(qint64));
    memcpy(main_buffer.data() + MACBYTES + sizeof(quint64), src_name.constData(), len);

    // next, encode the header size and put it into the destination
    quint64 header_size = MACBYTES + sizeof(quint64) + len;
    qToLittleEndian<quint64>(header_size, qint64_buffer.data() + MACBYTES);

    std::unique_ptr<Botan::Cipher_Mode> enc = Botan::Cipher_Mode::create("ChaCha20/Poly1305", Botan::ENCRYPTION);
    enc->set_key(master_key);
    enc->start(nonce_buffer);
    enc->finish(qint64_buffer);

    if(des_stream.writeRawData(reinterpret_cast<char *>(qint64_buffer.data()), MACBYTES +
        sizeof(quint64)) <	static_cast<int>(MACBYTES + sizeof(quint64)))
        return "DES_HEADER_WRITE_ERROR";

    // finally, encrypt and write in the file size and filename
    Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
    enc->start(nonce_buffer);
    enc->finish(main_buffer);

    if(des_stream.writeRawData(reinterpret_cast<char *>(main_buffer.data()), header_size) < static_cast<int>(header_size))
        return "DES_HEADER_WRITE_ERROR";

    // now, move on to the actual data
    QDataStream src_stream(&src_file);

    qint64 curr_read = 0;
    double processed = 0;

    cout << "Encryption. Please be patient..." << endl;
    while(1)
    {

        len = src_stream.readRawData(reinterpret_cast<char *>(main_buffer.data() + MACBYTES),
            ARs::BUFFER_SIZE_WITHOUT_MAC);
        curr_read += len;

        if(len == -1)
            return ("DES_HEADER_WRITE_ERROR");

        else if(len == 0)
        {
            if(curr_read != filesize)
                return "SRC_BODY_READ_ERROR";
            else
                break;
        }
        else if(curr_read > filesize)
            return "SRC_BODY_READ_ERROR";

        // increment the nonce, encrypt and write the cipertext to the destination file
        Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
        enc->start(nonce_buffer);
        enc->finish(main_buffer);

        if(des_stream.writeRawData(reinterpret_cast<char *>(main_buffer.data()), len +
            MACBYTES) < len + MACBYTES)
            return "DES_BODY_WRITE_ERROR";

        processed += len;
        double p = (processed / filesize) * 100; // calculate percentage proccessed
        printProgress(p / 100); // show updated progress

    }
    cout << endl << "Successfully encrypted" << endl;
    return "CRYPT_SUCCESS";
}
