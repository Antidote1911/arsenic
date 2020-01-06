#include "divers.h"
#include <QString>
#include <QDir>
#include <QTextStream>
#include "botan_all.h"
#include "constants.h"
#include "messages.h"

using namespace ARs;
using namespace MyMessagesPublic;



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
    Botan::SecureVector<quint8> key_buffer(ARs::KEYBYTES*3);

    // mem,ops,threads
    auto default_pwhash = pwdhash_fam->from_params(memlimit,iterations,PARALLELISM_INTERACTIVE);

    default_pwhash->derive_key(key_buffer.data(), key_buffer.size(),
                               pass_buffer.data(), pass_buffer.size(),
                               salt_buffer.data(), salt_buffer.size());

    return key_buffer;
}

Botan::SecureVector<char> convertStringToSecureVector(QString password)
{
    Botan::SecureVector<char> pass_buffer(ARs::KEYBYTES);
    memset(pass_buffer.data(), 0, KEYBYTES);
    memcpy(pass_buffer.data(), password.toUtf8().constData(), std::min(password.toUtf8().size(),
        static_cast<int>(KEYBYTES)));
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
