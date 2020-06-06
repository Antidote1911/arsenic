#include "messages.h"
#include <QObject>
#include <QString>

QString errorCodeToString(int error_code)
{
    QString ret_string;
    switch (error_code) {
    case CRYPT_SUCCESS:
        ret_string += QObject::tr("Data successfully encrypted !");
        break;

    case DECRYPT_SUCCESS:
        ret_string += QObject::tr("Data successfully decrypted !");
        break;

    case NOT_AN_ARSENIC_FILE:
        ret_string += QObject::tr("This file is not an Arsenic File !");
        break;

    case SRC_NOT_FOUND:
        ret_string += QObject::tr("The file was not found !");
        break;

    case SRC_CANNOT_OPEN_READ:
        ret_string += QObject::tr("The file could not be opened for reading !");
        break;

    case PASS_HASH_FAIL:
        ret_string += QObject::tr("The password could not be hashed !");
        break;

    case DES_HEADER_ENCRYPT_ERROR:
        ret_string += QObject::tr("The intermediate file header could not be encrypted !");
        break;

    case DES_FILE_EXISTS:
        ret_string += QObject::tr("The encrypted file already exists !");
        break;

    case DES_CANNOT_OPEN_WRITE:
        ret_string += QObject::tr("The encrypted file could not be opened for writin !");
        break;

    case DES_HEADER_WRITE_ERROR:
        ret_string += QObject::tr("The encrypted file could not be written to !");
        break;

    case SRC_BODY_READ_ERROR:
        ret_string += QObject::tr("The file could not be read.");
        break;

    case DATA_ENCRYPT_ERROR:
        ret_string += QObject::tr("The file's data could not be encrypted.");
        break;

    case DES_BODY_WRITE_ERROR:
        ret_string += QObject::tr("The encrypted file could not be written to.");
        break;

    case SRC_HEADER_READ_ERROR:
        ret_string += QObject::tr("Can't read the header.");
        break;

    case SRC_HEADER_INVALID_TAG:
        ret_string += QObject::tr("Authentication Failure. Could not decrypt the header. Incorrect password or corrupted file.");
        break;

    case SRC_HEADER_INTEGRITY_FAILURE:
        ret_string += QObject::tr("Header Integrity Failure. Incorrect password or corrupted file.");
        break;

    case INVALID_TAG:
        ret_string += QObject::tr("Authentication Failure. Wrong password, or data are corrupted.");
        break;

    case INTEGRITY_FAILURE:
        ret_string += QObject::tr("File Integrity Failure. Data are corrupted.");
        break;

    case ABORTED_BY_USER:
        ret_string += QObject::tr("Aborted by user ! Intermediate file is deleted.");
        break;

    case INVALID_CRYPTOBOX_IMPUT:
        ret_string += QObject::tr("Invalid Arsenic CryptoBox input.");
        break;

    case BAD_CRYPTOBOX_VERSION:
        ret_string += QObject::tr("Bad Arsenic CryptoBox version.");
        break;

    case BAD_CRYPTOBOX_PEM_HEADER:
        ret_string += QObject::tr("Bad Arsenic CryptoBox header.");
        break;
    }
    return (ret_string);
}
