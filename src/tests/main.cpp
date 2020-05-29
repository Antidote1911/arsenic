#define CATCH_CONFIG_RUNNER
#include <QCoreApplication>
//#include <QDebug>
#include "../arsenic/constants.h"
#include "../arsenic/crypto.h"
#include "../arsenic/divers.h"
#include "catch.hpp"
#include <QDataStream>
#include <QDir>
#include <QFile>

#include <QtGlobal>
#if defined(Q_OS_UNIX)
    #include "botan_all.h"
#endif

#if defined(Q_OS_WIN)
    #include "botan_all.h"
#endif

using namespace ARs;

int main(int argc, char* argv[])
{
    QCoreApplication a(argc, argv);

    int result = Catch::Session().run(argc, argv);

    return(result < 0xff ? result : 0xff);
}

int Factorial(int number)
{
    return(number <= 1 ? number : Factorial(number - 1) * number); // fail
}

bool encryptString()
{
    QString plaintext = "my super secret message";
    QString encrypted = encryptString(plaintext, "mypassword");
    QString decrypted = decryptString(encrypted, "mypassword");
    return(plaintext == decrypted);
}

bool encryptFile()
{
    // We generate a ramdom file
    Botan::SecureVector<quint8> main_buffer(IN_BUFFER_SIZE);
    Botan::AutoSeeded_RNG rng;

    main_buffer = rng.random_vec(IN_BUFFER_SIZE);
    QFile::remove(QDir::cleanPath("cleartxt.txt")); //clear previous file
    QFile::remove(QDir::cleanPath("cleartxt.txt.arsenic"));

    QFile src_file(QDir::cleanPath("cleartxt.txt"));
    src_file.open(QIODevice::WriteOnly);
    QDataStream des_stream(&src_file);
    des_stream.writeRawData(reinterpret_cast<char*>(main_buffer.data()), main_buffer.size());
    src_file.close();

    // Calculate the SHA-256 of the generated file for future comparison
    std::unique_ptr<Botan::HashFunction> hash1(Botan::HashFunction::create("SHA-256"));
    src_file.open(QIODevice::ReadOnly);
    Botan::SecureVector<uint8_t> buf(IN_BUFFER_SIZE);
    src_file.read(reinterpret_cast<char*>(buf.data()), buf.size());
    hash1->update(buf.data(), buf.size());
    QString result1 = QString::fromStdString(Botan::hex_encode(hash1->final ()));

    // Now, try to encrypt it. The original is deleted, and the output is cleartxt.txt.arsenic
    QStringList list;
    list.append("cleartxt.txt");

    Crypto_Thread Crypto;
    Crypto.setParam(true,
                    list,
                    "mypassword",
                    DEFAULT_ARGON_MEM_LIMIT,
                    DEFAULT_ARGON_ITR_LIMIT,
                    true);

    Crypto.start();
    Crypto.wait();

    // Now, decrypt it. cleartxt.txt.arsenic was deleted after
    // the decryption to cleartxt.txt

    QStringList list2;
    list2.append("cleartxt.txt.arsenic");

    Crypto.setParam(false,
                    list2,
                    "mypassword",
                    DEFAULT_ARGON_MEM_LIMIT,
                    DEFAULT_ARGON_ITR_LIMIT,
                    true);

    Crypto.start();
    Crypto.wait();

    // Calculate the SHA-256 of the decrypted file for comparison with the original hash
    std::unique_ptr<Botan::HashFunction> hash2(Botan::HashFunction::create("SHA-256"));
    QFile src_file2(QDir::cleanPath("cleartxt.txt"));
    src_file2.open(QIODevice::ReadOnly);
    Botan::SecureVector<uint8_t> buf2(IN_BUFFER_SIZE);
    src_file2.read(reinterpret_cast<char*>(buf2.data()), buf2.size());
    hash2->update(buf2.data(), buf2.size());
    QString result2 = QString::fromStdString(Botan::hex_encode(hash2->final ()));

    return(result1 == result2);
}

QString upper(QString str)
{
    return(str.toUpper());
}

TEST_CASE("Factorials of 1 and higher are computed (pass)", "[single-file]")
{
    REQUIRE(Factorial(1) == 1);
    REQUIRE(Factorial(2) == 2);
    REQUIRE(Factorial(3) == 6);
    REQUIRE(Factorial(10) == 3628800);
}

TEST_CASE("Upper", "[single-file]")
{
    REQUIRE(upper("test") == "TEST");
}

TEST_CASE("File Encryption/decryption", "[single-file]")
{
    REQUIRE(encryptFile() == true);
}
TEST_CASE("String Encryption/decryption", "[single-file]")
{
    REQUIRE(encryptString() == true);
}
