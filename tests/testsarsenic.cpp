/*
 * This file is part of Arsenic.
 *
 * Copyright (C) 2017 Corraire Fabrice <antidote1911@gmail.com>
 *
 * Arsenic is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * Arsenic is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Arsenic. If not, see <http://www.gnu.org/licenses/>.
 */

#include <QtTest>
#include <iostream>
#include "../arsenic/crypto/argon2/argon2.h"
#include "../arsenic/crypto/Crypto.h"
#include "../arsenic/crypto/hash_tool.h"
#include "../arsenic/crypto/argonhash.h"


#if defined(Q_OS_LINUX)
#if defined(__clang__)
#include "../arsenic/crypto/botan/clang/botan_all.h"
#elif defined(__GNUC__) || defined(__GNUG__)
#include "../arsenic/crypto/botan/gcc/botan_all.h"
#endif
#endif

#if defined(Q_OS_WIN64)
#include "../arsenic/crypto/botan/msvc_x64/botan_all.h"
#endif

using namespace std;


class TestArsenic: public QObject
{
    Q_OBJECT
private slots:
    void initialTest();
    void testArgon2();
    void testHash();
    void testEncrypt();
    void testDecrypt();
};

void TestArsenic::initialTest()
{
    QString str = "Hello";
    QCOMPARE(str.toUpper(), QString("HELLO"));
}

void TestArsenic::testHash()
{
    Hash_Tool hash;
    QStringList fileList;

    const QString fileName1 = QStringLiteral("test1.txt");

    QFile file1{fileName1};
    if (!file1.exists())
    {
        QString message;
        auto msg = QStringLiteral("Test file %1 is missing. ");

        if (!file1.exists())
        {
            message += msg.arg(fileName1);
        }

        QSKIP(message.toStdString().c_str());

    }
    fileList.append(fileName1);
    hash.setParam(fileList,"SHA-256");


    hash.start();
    hash.wait();

}

void TestArsenic::testArgon2()
{
    // for this references parameters and outputs, see :
    // https://github.com/P-H-C/phc-winner-argon2/blob/master/README.md

    uint32_t time_cost     = 2;
    uint32_t memory_cost   = 1<<16; // 65536 KiB (64 MiB)
    uint32_t parallelism   = 4;
    size_t outputRawLen=24;

    string password  = "password";
    string salt      = "somesalt";

    string referenceResultEncoded = "$argon2i$v=19$m=65536,t=2,p=4$c29tZXNhbHQ$RdescudvJCsgt3ub+b+dWRWJTmaaJObG";
    string referenceResultRaw     = "45D7AC72E76F242B20B77B9BF9BF9D5915894E669A24E6C6";

    // ///////////////////////////////


    vector<uint8_t> outputRaw(outputRawLen);
    vector<char> outputEncoded(256);

    outputRaw     = pwdHashRaw(time_cost,memory_cost,parallelism,password,salt,outputRawLen);
    outputEncoded = pwdHashEncoded(time_cost,memory_cost,parallelism,password,salt,outputRawLen);

    QVERIFY(Botan::hex_encode(outputRaw) == referenceResultRaw);
    QVERIFY(outputEncoded.data()         == referenceResultEncoded);

}

void TestArsenic::testEncrypt()
{
    Crypto_Thread crypto;
    QStringList fileList;

    const QString fileName1 = QStringLiteral("test1.txt");

    QFile file1{fileName1};
    if (!file1.exists())
    {
        QString message;
        auto msg = QStringLiteral("Test file %1 is missing. ");

        if (!file1.exists())
        {
            message += msg.arg(fileName1);
        }

        QSKIP(message.toStdString().c_str());

    }

    QString password      = "my password";
    QString algo          = "Serpent/GCM";
    bool encrypt          = true;
    QString encoding      = "Base64_Encoder";
    bool deleteOriginals  = false;
    QString appVersion    = "0.9.9";

    fileList.append(fileName1);

    crypto.setParam(fileList, password, algo, encrypt, encoding, deleteOriginals, appVersion);
    crypto.start();
    crypto.wait();

    QFile file2{"test1.txt.ars"};
    QVERIFY(file2.exists());
}

void TestArsenic::testDecrypt()
{
    QString sha_256_original="9C759C9AA3CB6CE29A46C4EDA37FBFE29DA5F988FD5106A95B1377F657A3FD32";
    Crypto_Thread crypto;
    QStringList fileList;
    const QString fileName = QStringLiteral("test1.txt.ars");

    QFile file{fileName};
    if (!file.exists())
    {
        QString message;
        auto msg = QStringLiteral("encrypted Test file %1 is missing. ");

        if (!file.exists())
        {
            message += msg.arg(fileName);
        }

        QSKIP(message.toStdString().c_str());

    }
    QString password     = "my password";
    QString algo         = "not used for decrypt";
    bool encrypt         = false;
    QString encoding     = "not used for decrypt";
    bool deleteOriginals = false;
    QString appVersion   = "0.9.9";

    fileList.append(fileName);

    crypto.setParam(fileList, password, algo, encrypt, encoding, deleteOriginals, appVersion);
    crypto.start();
    crypto.wait();

    QFile file2{"test1 (2).txt"};
    QFile original{"test1.txt"};

    file2.open(QIODevice::ReadOnly);
    original.open(QIODevice::ReadOnly);
    bool verify = file2.readAll() == original.readAll();
    QVERIFY(verify);
    if (verify)
    {
        file.remove();
        file2.remove();
    }

}

QTEST_MAIN(TestArsenic)
#include "testsarsenic.moc"
