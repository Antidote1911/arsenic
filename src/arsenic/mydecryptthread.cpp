﻿#include "JlCompress.h"

#include <QtEndian>
#include <QDebug>

#include "mydecryptthread.h"
#include "mydecryptbar.h"
#include "myinterrupt.h"
#include "myfilesystemmodel.h"
#include "divers.h"
#include "constants.h"
#include "iostream"

#include "botan_all.h"

using namespace MyDecryptBarThreadPublic;
using namespace MyDecryptBarPublic;
using namespace MyAbstractBarPublic;
using namespace MyFileSystemModelPublic;
using namespace MyCryptMessagesPublic;

using namespace ARs;




/*******************************************************************************



*******************************************************************************/


MyDecryptThread::MyDecryptThread(MyFileSystemModel *arg_model, const QString &arg_password,bool
    arg_delete_success) : ptr_model(arg_model), password(arg_password),
	delete_success(arg_delete_success) {}


/*******************************************************************************



*******************************************************************************/


std::vector<int> MyDecryptThread::getStatus() const
{
	if(this->isRunning())
		return std::vector<int>();
	else
		return status_list;
}


/*******************************************************************************



*******************************************************************************/


const std::vector<MyFileInfoPtr> &MyDecryptThread::getItemList() const
{
	if(this->isRunning())
		return empty_list;
	else
		return item_list;
}


/*******************************************************************************



*******************************************************************************/


void MyDecryptThread::interruptionPoint()
{
	if(this->isInterruptionRequested())
	{
		ptr_model->moveToThread(this->thread());
		emit updateProgress(INTERRUPTED);
		throw MyInterrupt();
	}
}


/*******************************************************************************



*******************************************************************************/


void MyDecryptThread::run()
{
	try
	{
		runHelper();
	}
	catch(MyInterrupt &e)
	{
		return;
	}
}


/*******************************************************************************



*******************************************************************************/


void MyDecryptThread::runHelper()
{
	emit updateProgress(MIN);
    emit updateGeneralProgress(MIN);

	// clean up the model first
	ptr_model->removeEmpty();
	interruptionPoint();
	ptr_model->removeRootDir();
	interruptionPoint();
	ptr_model->removeRedundant();
	interruptionPoint();
    ptr_model->sorting();
	interruptionPoint();

	int n = ptr_model->rowCount();

	// store the paths of all the items from the model that are checked for decryption
	for(int i = 0; i < n; i++)
	{
		if(ptr_model->data(ptr_model->index(i, CHECKED_HEADER), Qt::CheckStateRole) == Qt::Checked)
		{
			QString full_path = ptr_model->data(ptr_model->index(i,	FULL_PATH_HEADER)).toString();
			item_list.push_back(MyFileInfoPtr(new MyFileInfo(nullptr, full_path)));
		}
	}

    int total_items = item_list.size();
	status_list.resize(total_items, NOT_STARTED);

	interruptionPoint();

	// remove any junk in the temp directory
	QString temp_path = QDir::cleanPath(QDir::currentPath() + "/temp");
	clearDir(temp_path);

	// go through each one and decrypt the file or directory, then unzip
	for(int i = 0; i < total_items; i++)
	{
		interruptionPoint();

		QString full_path = item_list[i]->getFullPath();
		QString decrypt_name;

		// make sure the item is a file with a .qtcrypt extension before attempting to decrypt

			// decrypt the file and save the resulting name
            emit updateStatusText("Decrypting " + full_path);
            int decrypt_ret_val = myDecryptFile(temp_path, full_path, password, &decrypt_name);

			QString decrypt_path = QDir::cleanPath(QDir::currentPath() + "/temp/" + decrypt_name);
			QString unzip_name = decrypt_name;
			unzip_name.chop(sizeof(".zip") - 1);
			QString unzip_path = QDir::cleanPath(item_list[i]->getPath() + "/" + unzip_name);

			// check if there was an error. if so, do nothing else
			if(decrypt_ret_val != CRYPT_SUCCESS)
				status_list[i] = decrypt_ret_val;
			// otherwise, decryption was successful. unzip the file into the encrypted file's directory
			else
			{
                emit updateStatusText("Unzipping " + full_path);
				QuaZip qz(decrypt_path);

				if(QFileInfo::exists(unzip_path))
					status_list[i] = ZIP_ERROR;
				else if(qz.open(QuaZip::mdUnzip))
				{
					// check what item type the zipped file is from the embedded comment. depending on the
					// type, change the directory to extract the zip to

					QString item_type = qz.getComment();
					QString unzip_dir;

					if(item_type == MyFileInfo::typeToString(MyFileInfoPublic::MFIT_FILE))
						unzip_dir = QDir::cleanPath(item_list[i]->getPath());
					else
						unzip_dir = QDir::cleanPath(item_list[i]->getPath() + "/" + unzip_name);

					qz.close();

					// unzip to target directory
					if(JlCompress::extractDir(decrypt_path, unzip_dir).size() != 0)
					{
						// unzipping was a success, process is finished. add in the decrypted item
						status_list[i] = CRYPT_SUCCESS;

						if(item_type == MyFileInfo::typeToString(MyFileInfoPublic::MFIT_FILE))
							ptr_model->insertItem(ptr_model->rowCount(), unzip_path);
						else
							ptr_model->insertItem(0, unzip_path);

						// remove the encrypted file from the model if the user chose the option to
						if(delete_success)
						{
							if(QFile::remove(full_path))
								ptr_model->removeItem(full_path);
						}
					}
					// otherwise, there was a problem unzipping the decrypted file
					else
						status_list[i] = ZIP_ERROR;
				}
				// could not open the decrypted zip file
				else
					status_list[i] = ZIP_ERROR;
			}

			// clean up the decrypted zip file
            emit updateProgress((MAX / total_items) * (i + 1));
			QFile::remove(decrypt_path);
		}
		// was not a qtcrypt file




	emit updateProgress(MAX);
	ptr_model->moveToThread(this->thread());

	emit updateProgress(FINISHED);
    emit updateStatusText("Decrypted succesfully ");
}

int MyDecryptThread::myDecryptFile(const QString &des_path, const QString &src_path, const QString &key, QString
    *decrypt_name)
{
    emit updateGeneralProgress(0);
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
        return SRC_NOT_FOUND;

    if (!src_file.open(QIODevice::ReadOnly))
        return SRC_CANNOT_OPEN_READ;

    // open the source file and extract the initial nonce and password salt
    QDataStream src_stream(&src_file);

    // Read and check the header
    quint32 magic;
    src_stream >> magic;
    if (magic != 0x41525345)
    {
        return NOT_ARSENIC_FILE;
    }

    // Read the version
    QString version;
    src_stream.setVersion(QDataStream::Qt_5_12);
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
    src_stream.setVersion(QDataStream::Qt_5_12);
    src_stream >> memlimit;

    qint32 iterations;
    src_stream.setVersion(QDataStream::Qt_5_12);
    src_stream >> iterations;

    // Read crypto algo parameters
    QString cryptoAlgo;
    src_stream.setVersion(QDataStream::Qt_5_12);
    src_stream >> cryptoAlgo;

    // Read additional data (username)
    QString userName;
    src_stream.setVersion(QDataStream::Qt_5_12);
    src_stream >> userName;

    std::unique_ptr<Botan::AEAD_Mode> dec = Botan::AEAD_Mode::create(cryptoAlgo.toStdString(), Botan::DECRYPTION);
    std::string add_data(userName.toStdString());
    std::vector<uint8_t> add(add_data.data(),add_data.data()+add_data.length());

    if(src_stream.readRawData(reinterpret_cast<char *>(nonce_buffer.data()), NONCEBYTES)
        < static_cast<int>(NONCEBYTES))
        return SRC_HEADER_READ_ERROR;

    if(src_stream.readRawData(reinterpret_cast<char *>(salt_buffer.data()),
        SALTBYTES) < static_cast<int>(SALTBYTES))
        return SRC_HEADER_READ_ERROR;

    // Convert the QString password to securevector
    pass_buffer = convertStringToSecureVector(key);

    // Calculate the encryption key with Argon2
    emit updateStatusText("Argon2 passphrase derivation... Please wait.");
    master_key = calculateHash(pass_buffer, salt_buffer, memlimit, iterations);
    const uint8_t* mk = master_key.begin().base();
    const Botan::SymmetricKey cipher_key1(mk, KEYBYTES);
    const Botan::SymmetricKey cipher_key2(&mk[KEYBYTES], KEYBYTES);
    const Botan::SymmetricKey cipher_key3(&mk[KEYBYTES+KEYBYTES], KEYBYTES);

    // next, get the encrypted header size and decrypt it
    qint64 header_size;

    src_stream.readRawData(reinterpret_cast<char *>(qint64_buffer.data()), 40);
    qDebug() << "qint64_buffer : "+ QString::fromStdString(Botan::hex_encode(qint64_buffer));

    dec->set_key(cipher_key1);
    dec->set_ad(add);
    dec->start(nonce_buffer);
    dec->finish(qint64_buffer);

    header_size = qFromLittleEndian<qint64>(qint64_buffer.data() + MACBYTES);

    if(header_size <= static_cast<int>(MACBYTES + sizeof(qint64)) || header_size > IN_BUFFER_SIZE)
        return SRC_HEADER_DECRYPT_ERROR;

    src_stream.readRawData(reinterpret_cast<char *>(main_buffer.data()), main_buffer.size());

    // get the file size and filename of original item
    Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
    dec->set_key(cipher_key2);
    dec->start(nonce_buffer);
    dec->finish(main_buffer);

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
        return DES_FILE_EXISTS;

    if(!des_file.open(QIODevice::WriteOnly))
        return DES_CANNOT_OPEN_WRITE;

    QDataStream des_stream(&des_file);

    // for percent progress calculation
    size_t fileindex = 0;
    qint64 percent = -1;
    ///////////////////////
    // start decrypting the actual data
    emit updateStatusText("Decryption... Please wait.");
    //std::unique_ptr<Botan::Cipher_Mode> dec2 = Botan::Cipher_Mode::create("ChaCha20/Poly1305", Botan::DECRYPTION);

    dec->set_key(cipher_key3);
    dec->set_ad(add);
    // increment the nonce, decrypt and write the plaintext block to the destination
    Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
    dec->start(nonce_buffer);
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
            dec->finish(main_buffer);
        }

            // Calculate progress in percent
            fileindex += len;
            const auto nextFraction = static_cast<double>(fileindex) /
                                      static_cast<double>(filesize);

            const qint64 nextPercent = static_cast<qint64>(nextFraction * 100);
            if (nextPercent > percent && nextPercent < 100)
            {
                percent = nextPercent;
                emit updateGeneralProgress(percent);
            }
        }

    emit updateGeneralProgress(100);
    emit updateStatusText("Encryption finished.");
    return CRYPT_SUCCESS;
}