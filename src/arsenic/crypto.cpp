#include "crypto.h"
#include "botan_all.h"
#include "constants.h"
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringBuilder>
#include <QThread>

using namespace ARs;
using namespace Botan;

Crypto_Thread::Crypto_Thread(QObject *parent) : QThread(parent) {}

void Crypto_Thread::setParam(bool direction, QStringList filenames,
                             const QString password, const QString algo,
                             int argonmem, int argoniter, bool deletefile) {
  m_filenames = filenames;
  m_password = password;
  m_algo = algo;
  m_argonmem = argonmem;
  m_argoniter = argoniter;
  m_direction = direction;
  m_deletefile = deletefile;
}

void Crypto_Thread::run() {
  for (auto &inputFileName : m_filenames) {

    if (aborted) {
      aborted = true;
      Crypto_Thread::terminate();
      return;
    }

    if (m_direction == true)

    {
      try {
        emit updateProgress(inputFileName, 0);
        emit statusMessage("");
        emit statusMessage(QDateTime::currentDateTime().toString(
                               "dddd dd MMMM yyyy (hh:mm:ss)") +
                           " encryption of " + inputFileName);

        encrypt(inputFileName);
        if (aborted) { // Reset abort flag
          aborted = false;
          return;
        }

      } catch (const Botan::Stream_IO_Error &) {
        emit statusMessage("Error: Botan::Stream_IO_Error");

      } catch (const std::exception &e) {
        const auto error = QString{e.what()};
        emit statusMessage(error);
      }

    }

    else {
      try {
        emit updateProgress(inputFileName, 0);
        emit statusMessage("");
        emit statusMessage(QDateTime::currentDateTime().toString(
                               "dddd dd MMMM yyyy (hh:mm:ss)") +
                           " decryption of " + inputFileName);
        decrypt(inputFileName);
        if (aborted) { // Reset abort flag
          aborted = false;
        }

      } catch (const Botan::Decoding_Error &) {
        emit statusMessage("Error: Can't decrypt file. Wrong password entered "
                           "or the file has been corrupted.");
        QFile::remove(outfileresult);

      } catch (const Botan::Integrity_Failure &) {
        emit statusMessage("Error: Can't decrypt file. Wrong password.");
        QFile::remove(outfileresult);

      } catch (const Botan::Invalid_Argument &) {
        emit statusMessage(
            "Error: Can't decrypt file. Is it an encrypted file ?");
        QFile::remove(outfileresult);

      } catch (const std::invalid_argument &) {
        emit statusMessage(
            "Error: Can't decrypt file. Is it an encrypted file ?");
        QFile::remove(outfileresult);

      } catch (const std::runtime_error &) {
        emit statusMessage("Error: Can't read file.");

      } catch (const std::exception &e) {
        const auto error = QString{e.what()};
        emit statusMessage("Error: " + error);
      }
    }
  }
}

void Crypto_Thread::encrypt(const QString src_path) {
  size_t memlimit = 0;
  size_t iterations = 0;

  if (m_argonmem == 0) {
    memlimit = ARs::MEMLIMIT_INTERACTIVE;
  }
  if (m_argonmem == 1) {
    memlimit = ARs::MEMLIMIT_MODERATE;
  }
  if (m_argonmem == 2) {
    memlimit = ARs::MEMLIMIT_SENSITIVE;
  }
  ////////////////////////////////////////////////
  if (m_argoniter == 0) {
    iterations = ARs::ITERATION_INTERACTIVE;
  }
  if (m_argoniter == 1) {
    iterations = ARs::ITERATION_MODERATE;
  }
  if (m_argoniter == 2) {
    iterations = ARs::ITERATION_SENSITIVE;
  }

  emit updateProgress(src_path, 0);
  Botan::SecureVector<quint8> main_buffer(IN_BUFFER_SIZE);
  Botan::SecureVector<char> pass_buffer(CIPHER_KEY_LEN);
  Botan::SecureVector<quint8> master_key(CIPHER_KEY_LEN *
                                         3); // 256 bits (32 bytes)
  Botan::SecureVector<quint8> nonce_buffer(
      CIPHER_IV_LEN); // 192 bits (24 bytes)
  Botan::SecureVector<quint8> salt_buffer(ARGON_SALT_LEN);
  Botan::SecureVector<quint8> qint64_buffer(MACBYTES + sizeof(qint64));

  std::unique_ptr<Botan::AEAD_Mode> enc =
      Botan::AEAD_Mode::create(m_algo.toStdString(), Botan::ENCRYPTION);
  std::string add_data(APP_URL.toStdString());
  std::vector<uint8_t> add(add_data.data(),
                           add_data.data() + add_data.length());

  int len;

  QFile src_file(QDir::cleanPath(src_path));
  QFileInfo src_info(src_file);

  if (!src_file.exists() || !src_info.isFile()) {
    emit statusMessage("Error: file exist ?");
    return;
  }

  if (!src_file.open(QIODevice::ReadOnly)) {
    emit statusMessage("Error: can't read file");
    return;
  }

  // Convert the QString password to securevector
  pass_buffer = convertStringToSecureVector(m_password);

  // Randomize the 16 bytes salt
  Botan::AutoSeeded_RNG rng;
  salt_buffer = rng.random_vec(ARGON_SALT_LEN);

  // Calculate the encryption key with Argon2
  emit statusMessage("Argon2 passphrase derivation... Please wait.");
  master_key = calculateHash(pass_buffer, salt_buffer, memlimit, iterations);
  // Split master_key in tree parts.
  const uint8_t *mk = master_key.begin().base();
  const Botan::SymmetricKey cipher_key1(mk, CIPHER_KEY_LEN);
  const Botan::SymmetricKey cipher_key2(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
  const Botan::SymmetricKey cipher_key3(&mk[CIPHER_KEY_LEN + CIPHER_KEY_LEN],
                                        CIPHER_KEY_LEN);

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

  // Write a header with a "magic number" , arsenic version, argon2 parameters,
  // crypto algorithm
  des_stream << static_cast<quint32>(ARs::MAGIC_NUMBER);
  des_stream << static_cast<QString>(ARs::APP_VERSION);
  des_stream << static_cast<qint32>(memlimit);
  des_stream << static_cast<qint32>(iterations);
  des_stream << static_cast<QString>(m_algo);
  des_stream.setVersion(QDataStream::Qt_5_0);

  // Generate a 24 bytes random initial nonce
  nonce_buffer = rng.random_vec(CIPHER_IV_LEN);

  // Write the initial nonce in file
  if (des_stream.writeRawData(reinterpret_cast<char *>(nonce_buffer.data()),
                              CIPHER_IV_LEN) < static_cast<int>(CIPHER_IV_LEN))
    return;

  // Write the salt in file
  if (des_stream.writeRawData(reinterpret_cast<char *>(salt_buffer.data()),
                              ARGON_SALT_LEN) <
      static_cast<int>(ARGON_SALT_LEN))
    return;

  // move the file size and name data into the main buffer
  QByteArray src_name = src_info.fileName().toUtf8();

  qint64 filesize = src_info.size();
  qToLittleEndian<qint64>(filesize, main_buffer.data() + MACBYTES);

  len = std::min<int>(static_cast<long unsigned int>(src_name.size()),
                      BUFFER_SIZE_WITHOUT_MAC - sizeof(qint64));
  memcpy(main_buffer.data() + MACBYTES + sizeof(quint64), src_name.constData(),
         len);

  // next, encode the header size and put it into the destination
  quint64 header_size = MACBYTES + sizeof(quint64) + len;
  qToLittleEndian<quint64>(header_size, qint64_buffer.data() + MACBYTES);

  enc->set_key(cipher_key1);
  enc->set_ad(add);
  enc->start(nonce_buffer);
  Botan::secure_vector<uint8_t> out_buf1(qint64_buffer.begin(),
                                         qint64_buffer.end());
  enc->finish(out_buf1); // 40
  des_stream.writeRawData(reinterpret_cast<char *>(out_buf1.data()),
                          out_buf1.size());

  // finally, encrypt and write in the file size and filename
  Botan::Sodium::sodium_increment(nonce_buffer.data(), CIPHER_IV_LEN);
  Botan::secure_vector<uint8_t> out_buf2(main_buffer.begin(),
                                         main_buffer.end());
  enc->set_key(cipher_key2);
  enc->start(nonce_buffer);
  enc->finish(out_buf2); // 65552

  des_stream.writeRawData(reinterpret_cast<char *>(out_buf2.data()),
                          out_buf2.size());

  // now, move on to the actual data
  QDataStream src_stream(&src_file);

  ///////////////////////
  emit statusMessage("Encryption... Please wait...");
  // std::unique_ptr<Botan::AEAD_Mode> enc2 =
  // Botan::AEAD_Mode::create("ChaCha20Poly1305", Botan::ENCRYPTION);

  int bytes_read;
  Botan::SecureVector<uint8_t> buf(10 * 1024);
  const int block_size = 10 * 1024;

  Botan::Sodium::sodium_increment(nonce_buffer.data(), CIPHER_IV_LEN);
  enc->set_key(cipher_key3);
  enc->set_ad(add);
  enc->start(nonce_buffer);

  double processed = 0;
  while (!aborted &&
         (bytes_read = src_stream.readRawData(
              reinterpret_cast<char *>(buf.data()), block_size)) > 0) {

    if (bytes_read < block_size) {
      buf.resize(bytes_read);
      enc->finish(buf);
      des_stream.writeRawData(reinterpret_cast<char *>(buf.data()), buf.size());
    } else {
      enc->update(buf); // 65536
      des_stream.writeRawData(reinterpret_cast<char *>(buf.data()), bytes_read);

      // calculate percentage proccessed
      processed += bytes_read;
      double p =
          (processed / filesize) * 100; // calculate percentage proccessed
      emit updateProgress(src_path, p);
    }
  }

  if (aborted) {
    emit statusMessage("Encryption aborted by user. The incomplete encrypted "
                       "file was deleted.");
    emit updateProgress(src_path, 0);
    des_file.remove();
    return;
  }

  emit updateProgress(src_path, 100);
  emit statusMessage("Encryption finished.");
  QFileInfo fileInfo(des_file.fileName());

  if (m_deletefile == true) {
    QFile::remove(src_path);
    emit sourceDeletedAfterSuccess(src_path);
  }

  emit sucessMessage(fileInfo.filePath());
}

void Crypto_Thread::decrypt(QString src_path) {
  emit updateProgress(src_path, 0);
  SecureVector<quint8> main_buffer(IN_BUFFER_SIZE + MACBYTES);
  SecureVector<quint8> master_key(CIPHER_KEY_LEN * 3);
  SecureVector<char> pass_buffer(CIPHER_KEY_LEN);
  SecureVector<quint8> nonce_buffer(CIPHER_IV_LEN);
  SecureVector<quint8> salt_buffer(ARGON_SALT_LEN);
  SecureVector<quint8> qint64_buffer(40);
  qint64 len;

  QFile src_file(QDir::cleanPath(src_path));
  QFileInfo src_info(src_file);

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
  qint32 memlimit;
  src_stream >> memlimit;

  qint32 iterations;
  src_stream >> iterations;

  // Read crypto algo parameters
  QString cryptoAlgo;
  src_stream >> cryptoAlgo;

  std::unique_ptr<Botan::AEAD_Mode> dec =
      Botan::AEAD_Mode::create(cryptoAlgo.toStdString(), Botan::DECRYPTION);
  std::string add_data(APP_URL.toStdString());
  std::vector<uint8_t> add(add_data.data(),
                           add_data.data() + add_data.length());

  if (src_stream.readRawData(reinterpret_cast<char *>(nonce_buffer.data()),
                             CIPHER_IV_LEN) < static_cast<int>(CIPHER_IV_LEN))
    return;

  if (src_stream.readRawData(reinterpret_cast<char *>(salt_buffer.data()),
                             ARGON_SALT_LEN) < static_cast<int>(ARGON_SALT_LEN))
    return;

  // Convert the QString password to securevector
  pass_buffer = convertStringToSecureVector(m_password);

  // Calculate the encryption key with Argon2
  // emit updateStatusText("Argon2 passphrase derivation... Please wait.");
  master_key = calculateHash(pass_buffer, salt_buffer, memlimit, iterations);
  const uint8_t *mk = master_key.begin().base();
  const Botan::SymmetricKey cipher_key1(mk, CIPHER_KEY_LEN);
  const Botan::SymmetricKey cipher_key2(&mk[CIPHER_KEY_LEN], CIPHER_KEY_LEN);
  const Botan::SymmetricKey cipher_key3(&mk[CIPHER_KEY_LEN + CIPHER_KEY_LEN],
                                        CIPHER_KEY_LEN);

  // next, get the encrypted header size and decrypt it
  qint64 header_size;

  if (src_stream.readRawData(reinterpret_cast<char *>(qint64_buffer.data()),
                             40) < 40)
    return;

  dec->set_key(cipher_key1);
  dec->set_ad(add);
  dec->start(nonce_buffer);

  try {
    dec->finish(qint64_buffer);
  } catch (Botan::Exception &e) {
    QString error = e.what();
    emit statusMessage("Error: " + error);
    return;
  }

  header_size = qFromLittleEndian<qint64>(qint64_buffer.data() + MACBYTES);

  src_stream.readRawData(reinterpret_cast<char *>(main_buffer.data()),
                         main_buffer.size());

  // get the file size and filename of original item
  Botan::Sodium::sodium_increment(nonce_buffer.data(), CIPHER_IV_LEN);
  dec->set_key(cipher_key2);
  dec->start(nonce_buffer);

  try {
    dec->finish(main_buffer);
  } catch (Botan::Exception &e) {
    QString error = e.what();
    emit statusMessage("Error: " + error);
    return;
  }

  qint64 filesize = qFromLittleEndian<qint64>(main_buffer.data() + MACBYTES);
  len = header_size - MACBYTES - sizeof(qint64);

  QString des_name = QString::fromUtf8(
      reinterpret_cast<char *>(main_buffer.data() + MACBYTES + sizeof(qint64)),
      len);

  // create the decrypted file in the passed directory
  QFile des_file(QDir::cleanPath(src_info.absolutePath() + "/" + des_name));

  if (des_file.exists()) {
    emit statusMessage("Error: the output file already exist");
    return;
  }

  if (!des_file.open(QIODevice::WriteOnly)) {
    emit statusMessage("Error: the output file can't be open for writing");
    return;
  }

  QDataStream des_stream(&des_file);

  // start decrypting the actual data
  emit statusMessage("Decryption... Please wait.");

  // increment the nonce, decrypt and write the plaintext block to the
  // destination

  double processed = 0;

  int bytes_read;

  Botan::SecureVector<uint8_t> buf(10 * 1024);
  const int block_size = 10 * 1024;

  Botan::Sodium::sodium_increment(nonce_buffer.data(), CIPHER_IV_LEN);
  dec->set_key(cipher_key3);
  dec->set_ad(add);
  dec->start(nonce_buffer);

  while (!aborted &&
         (bytes_read = src_stream.readRawData(
              reinterpret_cast<char *>(buf.data()), block_size)) > 0) {
    if (bytes_read < block_size) {
      buf.resize(bytes_read);
      dec->finish(buf);
      des_stream.writeRawData(reinterpret_cast<char *>(buf.data()), buf.size());
    } else {
      dec->update(buf); // 65536
      des_stream.writeRawData(reinterpret_cast<char *>(buf.data()), bytes_read);

      // calculate percentage proccessed
      processed += bytes_read;
      double p = (processed / filesize) * 100;
      emit updateProgress(src_path, p);
    }
  }

  emit updateProgress(src_path, 100);

  if (m_deletefile == true) {
    QFile::remove(src_path);
    emit sourceDeletedAfterSuccess(src_path);
  }

  QFileInfo des_info(des_file);
  emit sucessMessage(des_info.filePath());
  emit statusMessage("Decryption finished.");
}

Botan::SecureVector<char>
Crypto_Thread::convertStringToSecureVector(QString password) {
  Botan::SecureVector<char> pass_buffer(CIPHER_KEY_LEN);
  memset(pass_buffer.data(), 0, CIPHER_KEY_LEN);
  memcpy(pass_buffer.data(), password.toUtf8().constData(),
         std::min(password.toUtf8().size(), static_cast<int>(CIPHER_KEY_LEN)));
  return pass_buffer;
}

Botan::SecureVector<quint8>
Crypto_Thread::calculateHash(Botan::SecureVector<char> pass_buffer,
                             Botan::SecureVector<quint8> salt_buffer,
                             size_t memlimit, size_t iterations) {
  auto pwdhash_fam = Botan::PasswordHashFamily::create("Argon2id");
  Botan::SecureVector<quint8> key_buffer(CIPHER_KEY_LEN * 3);

  // mem,ops,threads
  auto default_pwhash =
      pwdhash_fam->from_params(memlimit, iterations, PARALLELISM_INTERACTIVE);

  default_pwhash->derive_key(key_buffer.data(), key_buffer.size(),
                             pass_buffer.data(), pass_buffer.size(),
                             salt_buffer.data(), salt_buffer.size());
  return key_buffer;
}

QString Crypto_Thread::removeExtension(const QString &fileName,
                                       const QString &extension) {
  QFileInfo file{fileName};
  QString newFileName = fileName;

  if (file.suffix() == extension) {
    newFileName =
        file.absolutePath() % QDir::separator() % file.completeBaseName();
  }

  return newFileName;
}

QString Crypto_Thread::uniqueFileName(const QString &fileName) {
  QFileInfo originalFile{fileName};
  QString uniqueFileName = fileName;

  auto foundUniqueFileName = false;
  auto i = 0;

  while (!foundUniqueFileName && i < 100000) {
    QFileInfo uniqueFile{uniqueFileName};

    if (uniqueFile.exists() &&
        uniqueFile.isFile()) { // Write number of copies before file extension
      uniqueFileName = originalFile.absolutePath() % QDir::separator() %
                       originalFile.baseName() % QString{" (%1)"}.arg(i + 2);

      if (!originalFile.completeSuffix()
               .isEmpty()) { // Add the file extension if there is one
        uniqueFileName += QStringLiteral(".") % originalFile.completeSuffix();
      }

      ++i;
    } else {
      foundUniqueFileName = true;
    }
  }
  return uniqueFileName;
}
