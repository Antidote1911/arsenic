#define CATCH_CONFIG_RUNNER
#include <QCoreApplication>
//#include <QDebug>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include "../arsenic/constants.h"
#include "../arsenic/fileCrypto.h"
#include "../arsenic/textcrypto.h"
#include "botan_all.h"
#include "catch.hpp"

using namespace ARs;

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);
    int result = Catch::Session().run(argc, argv);
    return (result < 0xff ? result : 0xff);
}

int Factorial(int number) {
    return (number <= 1 ? number : Factorial(number - 1) * number);  // fail
}

bool encryptString() {
    QString plaintext = "my super secret message";
    QString password = "mypassword";

    textCrypto encrypt;
    encrypt.start(password, true);
    encrypt.finish(plaintext);

    textCrypto decrypt;
    decrypt.start(password, false);
    decrypt.finish(plaintext);

    return (plaintext == "my super secret message");
}

bool encryptFile() {
    // We generate a ramdom file
    Botan::SecureVector<quint8> main_buffer(100000);
    Botan::AutoSeeded_RNG rng;

    main_buffer = rng.random_vec(100000);
    QFile::remove(QDir::cleanPath("cleartxt.txt"));  // clear previous file
    QFile::remove(QDir::cleanPath("cleartxt.txt.arsenic"));

    QFile src_file(QDir::cleanPath("cleartxt.txt"));
    src_file.open(QIODevice::WriteOnly);
    QDataStream des_stream(&src_file);
    des_stream.writeRawData(reinterpret_cast<char*>(main_buffer.data()), main_buffer.size());
    src_file.close();

    // Calculate the SHA-256 of the generated file for future comparison
    std::unique_ptr<Botan::HashFunction> hash1(Botan::HashFunction::create("SHA-256"));
    src_file.open(QIODevice::ReadOnly);
    QDataStream stream(&src_file);
    Botan::SecureVector<uint8_t> buf(IN_BUFFER_SIZE);
    quint32 bytes_read;
    while ((bytes_read = stream.readRawData(reinterpret_cast<char*>(buf.data()), IN_BUFFER_SIZE)) > 0) {
        hash1->update(buf.data(), buf.size());
    }
    QString result1 = QString::fromStdString(Botan::hex_encode(hash1->final()));

    // Now, try to encrypt it. The original is deleted, and the output is
    // cleartxt.txt.arsenic
    QStringList list;
    list.append("cleartxt.txt");

    Crypto_Thread Crypto;
    Crypto.setParam(true, list, "mypassword", DEFAULT_ARGON_MEM_LIMIT, DEFAULT_ARGON_ITR_LIMIT, true);

    Crypto.start();
    Crypto.wait();

    // Now, decrypt it. cleartxt.txt.arsenic was deleted after
    // the decryption to cleartxt.txt

    QStringList list2;
    list2.append("cleartxt.txt.arsenic");

    Crypto.setParam(false, list2, "mypassword", DEFAULT_ARGON_MEM_LIMIT, DEFAULT_ARGON_ITR_LIMIT, true);

    Crypto.start();
    Crypto.wait();

    // Calculate the SHA-256 of the decrypted file for comparison with the
    // original hash
    std::unique_ptr<Botan::HashFunction> hash2(Botan::HashFunction::create("SHA-256"));
    QFile src_file2(QDir::cleanPath("cleartxt.txt"));
    src_file2.open(QIODevice::ReadOnly);
    QDataStream stream2(&src_file2);
    Botan::SecureVector<uint8_t> buf2(IN_BUFFER_SIZE);
    quint32 bytes_read2;
    while ((bytes_read2 = stream2.readRawData(reinterpret_cast<char*>(buf2.data()), IN_BUFFER_SIZE)) > 0) {
        hash2->update(buf2.data(), buf2.size());
    }
    QString result2 = QString::fromStdString(Botan::hex_encode(hash2->final()));

    return (result1 == result2);
}

QString upper(QString str) {
    return (str.toUpper());
}

TEST_CASE("Factorials of 1 and higher are computed(pass) ", "[single - file] ") {
    REQUIRE(Factorial(1) == 1);
    REQUIRE(Factorial(2) == 2);
    REQUIRE(Factorial(3) == 6);
    REQUIRE(Factorial(10) == 3628800);
}

TEST_CASE("Upper ", "[single - file] ") {
    REQUIRE(upper("test") == "TEST");
}

TEST_CASE("File Encryption / decryption ", "[single - file] ") {
    REQUIRE(encryptFile() == true);
}
TEST_CASE("String Encryption / decryption ", "[single - file] ") {
    REQUIRE(encryptString() == true);
}
