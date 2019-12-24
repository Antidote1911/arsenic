#include <unordered_map>
#include <string>
#include <QtEndian>
#include <QDebug>

#include "JlCompress.h"

#include "myencryptthread.h"
#include "myencryptbar.h"
#include "myinterrupt.h"
#include "myfilesystemmodel.h"
#include "divers.h"
#include "constants.h"

#include "botan_all.h"


using namespace MyEncryptBarThreadPublic;
using namespace MyEncryptBarPublic;
using namespace MyAbstractBarPublic;
using namespace MyFileSystemModelPublic;
using namespace MyCryptMessagesPublic;

using namespace ARs;


/**********************************************************************************/

MyEncryptThread::MyEncryptThread(MyFileSystemModel *arg_model, const QString &arg_password, const QString &arg_userName,QString ext,QString cryptoAlgo,int argonMemory,int argonOps,bool
    arg_delete_success, const QString *arg_encrypt_name) : ptr_model(arg_model),
    password(arg_password),userName(arg_userName),ext(ext),cryptoAlgo(cryptoAlgo),argonmem(argonMemory),argonops(argonOps),delete_success(arg_delete_success), encrypt_name(arg_encrypt_name) {}

/**********************************************************************************

***********************************************************************************/

std::vector<int> MyEncryptThread::getStatus() const
{
	if(this->isRunning())
		return std::vector<int>();
	else
		return status_list;
}

/**********************************************************************************

***********************************************************************************/

const std::vector<MyFileInfoPtr> &MyEncryptThread::getItemList() const
{
	if(this->isRunning())
		return empty_list;
	else
		return item_list;
}

/**********************************************************************************

***********************************************************************************/

void MyEncryptThread::interruptionPoint()
{
	if(this->isInterruptionRequested())
	{
		ptr_model->moveToThread(this->thread());
		emit updateProgress(INTERRUPTED);
		throw MyInterrupt();
	}
}

/**********************************************************************************

***********************************************************************************/

void MyEncryptThread::run()
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

/**********************************************************************************

***********************************************************************************/

void MyEncryptThread::runHelper()
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

	// store the paths of all the items from the model that are checked for encryption
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

	// for use in naming the encrypted files
	int name_suffix;
	std::unordered_map<std::string, int> suffix_map(total_items);

	// remove any junk in the temp directory
	QString temp_path = QDir::cleanPath(QDir::currentPath() + "/temp");
	clearDir(temp_path);

	// go through each one and encrypt the file or directory after zipping
	for(int i = 0; i < total_items; i++)
	{
		interruptionPoint();

		QString full_path = item_list[i]->getFullPath();
		QString item_type = item_list[i]->getType();

		QString zip_path = QDir::cleanPath(QDir::currentPath() + "/temp/" + item_list[i]->getName() +
			".zip");

		bool ret_val = false;

        QFile src_file(QDir::cleanPath(full_path));
        QFileInfo src_info(src_file);
        QByteArray src_name = src_info.fileName().toUtf8();
        emit updateStatusText("Compressing " + src_name);
		// make sure the item exists

		if(item_type != MyFileInfo::typeToString(MyFileInfoPublic::MFIT_EMPTY))
		{
			QuaZip qz(zip_path);

			// try and compress the file or directory
			if(item_type == MyFileInfo::typeToString(MyFileInfoPublic::MFIT_FILE))
			{
				ret_val = JlCompress::compressFile(zip_path, full_path);                

				// if the zip file was created, add a comment on what type it originally was
				if(ret_val)
				{
					qz.open(QuaZip::mdAdd);
                    qz.setComment(MyFileInfo::typeToString(MyFileInfoPublic::MFIT_FILE));
					qz.close();
				}
			}
			else
			{
                qint64 size = dirSize(full_path);
                qInfo() << "folder size is: " + formatSize(size);



                    ret_val = JlCompress::compressDir(zip_path, full_path);

                    if(ret_val)
                    {
                        qz.open(QuaZip::mdAdd);
                        qz.setComment(MyFileInfo::typeToString(MyFileInfoPublic::MFIT_DIR));
                        qz.close();
                    }

			}

			// if successful, try encrypting the zipped file
			if(ret_val)
			{
				QString encrypt_path;

				// create the base name of the final encrypted file. defaults to the original filename
				// unless the user specified something for encrypt_name
				if(encrypt_name == nullptr)
                    encrypt_path = QDir::cleanPath(full_path + "."+ext);
				else
				{
					QString item_path = item_list[i]->getPath();

					// find the current number suffix for the given directory. 0 if it's the first time
					name_suffix = suffix_map[item_path.toStdString()];

					if(name_suffix != -1)
					{
						// get the next available filename, -1 if none is found within max tries
                        name_suffix = getNextFilenameSuffix(item_path, *encrypt_name + "_",	"."+ext,
							name_suffix);
					}

                    encrypt_path = QDir::cleanPath(item_path + "/" + *encrypt_name + "_" +
                        QString::number(name_suffix) + "."+ext);
					suffix_map[item_path.toStdString()] = name_suffix;
				}

				// make sure that a file with the same name as the encrypted doesn't already exist
				if(!QFile::exists(encrypt_path) && name_suffix != -1)
				{
                    status_list[i] = myEncryptFile(encrypt_path, zip_path, password,userName ,argonmem, argonops,cryptoAlgo);

					// if there was an error, delete the unfinished encryption file
					if(status_list[i] != CRYPT_SUCCESS)
						QFile::remove(encrypt_path);
					// otherwise, successful. add the encrypted item to the model
					else
					{
						ptr_model->insertItem(ptr_model->rowCount(), encrypt_path);

						// remove the old item if the user selected the option to
						if(delete_success)
						{
							if(item_type == MyFileInfo::typeToString(MyFileInfoPublic::MFIT_FILE))
							{
								if(QFile::remove(full_path))
									ptr_model->removeItem(full_path);
							}
							else
							{
								if(QDir(full_path).removeRecursively())
									ptr_model->removeItem(full_path);
							}
						}

						// status_list[i] == CRYPT_SUCCESS
					}
				}
				// the encrypted file already exists. do nothing and assign an error code
				else
					status_list[i] = DES_FILE_EXISTS;

				// delete the intermediate zip file when done                
                QFile::remove(zip_path);
			}
			// could not create the intermediate zip file
			else
				status_list[i] = ZIP_ERROR;
		}

		// item_type was empty, meaning the file or directory was not found
		else
			status_list[i] = SRC_NOT_FOUND;

		emit updateProgress((MAX / total_items) * (i + 1));
	}

	emit updateProgress(MAX);
	ptr_model->moveToThread(this->thread());
	emit updateProgress(FINISHED);
}

/**********************************************************************************

***********************************************************************************/

int MyEncryptThread::getNextFilenameSuffix(const QString &path, const QString &base_name, const
	QString &file_type, int start_num, int max_tries) const
{
	QDir qd(path);

	// check if the directory exists
	if(qd.exists())
	{
		// try to find the first name + number that isn't already used
		for(int i = 0; i < max_tries; i++)
		{
			int curr_num = start_num + i;
			QString full_name = base_name + QString::number(curr_num) + file_type;

			if(!qd.exists(full_name))
				return curr_num;
		}

		// no free natural number suffix was found
		return -1;
	}
	// the directory does not exist, thus 0 will work
	else
		return 0;
}

/**********************************************************************************

***********************************************************************************/

int MyEncryptThread::myEncryptFile(const QString &des_path, const QString &src_path, const QString &key,const QString &userName ,int argonmem,int iter, QString cryptoAlgo)
{
    size_t memlimit=0;
    size_t iterations=0;

    if (argonmem==0)
    {
        memlimit=ARs::MEMLIMIT_INTERACTIVE;
    }
    if (argonmem==1)
    {
        memlimit=ARs::MEMLIMIT_MODERATE;
    }
    if (argonmem==2)
    {
        memlimit=ARs::MEMLIMIT_SENSITIVE;
    }
    ////////////////////////////////////////////////
    if (iter==0)
    {
        iterations=ARs::ITERATION_INTERACTIVE;
    }
    if (iter==1)
    {
        iterations=ARs::ITERATION_MODERATE;
    }
    if (iter==2)
    {
        iterations=ARs::ITERATION_SENSITIVE;
    }

    emit updateGeneralProgress(0);
    Botan::SecureVector<quint8> main_buffer(IN_BUFFER_SIZE);
    Botan::SecureVector<char> pass_buffer(ARs::KEYBYTES);
    Botan::SecureVector<quint8> master_key(ARs::KEYBYTES*3);     // 256 bits (32 bytes)
    Botan::SecureVector<quint8> nonce_buffer(ARs::NONCEBYTES); // 192 bits (24 bytes)
    Botan::SecureVector<quint8> salt_buffer(ARs::SALTBYTES);
    Botan::SecureVector<quint8> qint64_buffer(ARs::MACBYTES + sizeof(qint64));

    std::unique_ptr<Botan::AEAD_Mode> enc = Botan::AEAD_Mode::create(cryptoAlgo.toStdString(), Botan::ENCRYPTION);
    std::string add_data(userName.toStdString());
    std::vector<uint8_t> add(add_data.data(),add_data.data()+add_data.length());

    qDebug() << qint64_buffer.size();
    int len;

    QFile src_file(QDir::cleanPath(src_path));
    QFileInfo src_info(src_file);

    if(!src_file.exists() || !src_info.isFile())
        return SRC_NOT_FOUND;

    if (!src_file.open(QIODevice::ReadOnly))
        return SRC_CANNOT_OPEN_READ;

    // Convert the QString password to securevector
    pass_buffer = convertStringToSecureVector(key);

    // Randomize the 16 bytes salt
    Botan::AutoSeeded_RNG rng;
    salt_buffer = rng.random_vec(SALTBYTES);

    // Calculate the encryption key with Argon2
    emit updateStatusText("Argon2 passphrase derivation... Please wait.");
    master_key = calculateHash(pass_buffer, salt_buffer, memlimit, iterations);
    // Split master_key in tree parts.
            const uint8_t* mk = master_key.begin().base();
            const Botan::SymmetricKey cipher_key1(mk, KEYBYTES);
            const Botan::SymmetricKey cipher_key2(&mk[KEYBYTES], KEYBYTES);
            const Botan::SymmetricKey cipher_key3(&mk[KEYBYTES+KEYBYTES], KEYBYTES);

            qDebug() << "master key : "+ QString::fromStdString(Botan::hex_encode(master_key));
            qDebug() << "cipher_key1 : "+ QString::fromStdString(Botan::hex_encode(cipher_key1.bits_of()));
            qDebug() << "cipher_key2 : "+ QString::fromStdString(Botan::hex_encode(cipher_key2.bits_of()));
            qDebug() << "cipher_key3 : "+ QString::fromStdString(Botan::hex_encode(cipher_key3.bits_of()));

    // create the new file, the path should normally be to the same directory as the source
    QFile des_file(QDir::cleanPath(des_path));

    if(des_file.exists())
        return DES_FILE_EXISTS;

    if(!des_file.open(QIODevice::WriteOnly))
        return DES_CANNOT_OPEN_WRITE;

    QDataStream des_stream(&des_file);

    // Write a header with a "magic number" , arsenic version, argon2 parameters, crypto algorithm
    des_stream << static_cast<quint32> (ARs::MAGIC_NUMBER);
    des_stream << static_cast<QString> (ARs::APP_VERSION);
    des_stream << static_cast<qint32>(memlimit);
    des_stream << static_cast<qint32>(iterations);
    des_stream << static_cast<QString>(cryptoAlgo);
    des_stream << static_cast<QString>(userName);
    des_stream.setVersion(QDataStream::Qt_5_0);

    // Generate a 24 bytes random initial nonce
    nonce_buffer = rng.random_vec(NONCEBYTES);

    // Write the initial nonce in file
    if(des_stream.writeRawData(reinterpret_cast<char *>(nonce_buffer.data()), NONCEBYTES)
        < static_cast<int>(NONCEBYTES))
        return DES_HEADER_WRITE_ERROR;

    // Write the salt in file
    if(des_stream.writeRawData(reinterpret_cast<char *>(salt_buffer.data()),
        SALTBYTES) < static_cast<int>(SALTBYTES))
        return DES_HEADER_WRITE_ERROR;

    // move the file size and name data into the main buffer
    QByteArray src_name = src_info.fileName().toUtf8();

    qint64 filesize = src_info.size();
    qToLittleEndian<qint64>(filesize, main_buffer.data() + MACBYTES);

    len = std::min<int>(static_cast<long unsigned int>(src_name.size()), BUFFER_SIZE_WITHOUT_MAC -
        sizeof(qint64));
    memcpy(main_buffer.data() + MACBYTES + sizeof(quint64), src_name.constData(), len);

    // next, encode the header size and put it into the destination
    quint64 header_size = MACBYTES + sizeof(quint64) + len;
    qToLittleEndian<quint64>(header_size, qint64_buffer.data() + MACBYTES);

    enc->set_key(cipher_key1);
    enc->set_ad(add);
    enc->start(nonce_buffer);
    Botan::secure_vector<uint8_t> out_buf1(qint64_buffer.begin(), qint64_buffer.end());
    enc->finish(out_buf1); // 40
    des_stream.writeRawData(reinterpret_cast<char *>(out_buf1.data()),out_buf1.size());

    // finally, encrypt and write in the file size and filename
    Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
    Botan::secure_vector<uint8_t> out_buf2(main_buffer.begin(), main_buffer.end());
    enc->set_key(cipher_key2);
    enc->start(nonce_buffer);
    enc->finish(out_buf2); // 65552

    des_stream.writeRawData(reinterpret_cast<char *>(out_buf2.data()), out_buf2.size());

    // now, move on to the actual data
    QDataStream src_stream(&src_file);

    ///////////////////////
    emit updateStatusText("Encryption... Please wait.");
    //std::unique_ptr<Botan::AEAD_Mode> enc2 = Botan::AEAD_Mode::create("ChaCha20Poly1305", Botan::ENCRYPTION);
    enc->set_key(cipher_key3);
    enc->set_ad(add);
    Botan::Sodium::sodium_increment(nonce_buffer.data(), NONCEBYTES);
    enc->start(nonce_buffer);
    double processed = 0;
    while(!src_stream.atEnd())
    {

            len = src_stream.readRawData(reinterpret_cast<char *>(main_buffer.data()), IN_BUFFER_SIZE);

            // increment the nonce, encrypt and write the cipertext to the destination file

            Botan::secure_vector<uint8_t> buf3(main_buffer.begin(), main_buffer.end());            
            if (!src_stream.atEnd())
            {
                enc->update(buf3);  //65536
                des_stream.writeRawData(reinterpret_cast<char *>(buf3.data()), buf3.size());
            }

            if (src_stream.atEnd())
            {
                enc->finish(buf3);  //65536+16
                des_stream.writeRawData(reinterpret_cast<char *>(buf3.data()), buf3.size());
            }

            // Calculate and display progress in percent
            processed += len;
            double p = (processed / filesize) * 100; // calculate percentage proccessed
            emit updateGeneralProgress(p); // show updated progress

        }

    emit updateGeneralProgress(100);
    emit updateStatusText("Encryption finished.");
    return CRYPT_SUCCESS;
}
