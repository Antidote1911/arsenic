#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QDebug>
#include <QObject>
#include <QtCore>
#include "utils.h"
#include "../core/consts.h"
#include "../core/fileCrypto.h"
#include <iostream>

QTextStream cout(stdout);
QTextStream cin(stdin);
consts constants;

void greeting() {
    const char *breakLine = "############################################\n";

    cout << Qt::endl;
    cout << breakLine;
    cout << constants.APP_LONG_NAME << Qt::endl;
    cout << breakLine;
    //cout << "V: " << constants.APP_VERSION << Qt::endl;
    cout << Qt::endl;
}

void onMessageChanged(QString message)
{
        cout << message << Qt::endl;
}


int main(int argc, char *argv[])
{
    QTextStream cin(stdin);
        QTextStream cout(stdout);

        QCoreApplication app(argc, argv);
        app.setOrganizationName(constants.APP_LONG_NAME);
        app.setOrganizationDomain(constants.APP_URL);
        app.setApplicationName(constants.APP_SHORT_NAME);
        app.setApplicationVersion(constants.APP_VERSION.toString());

        QCommandLineParser parser;
        parser.setApplicationDescription(constants.APP_DESCRIPTION);
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addPositionalArgument("source", QCoreApplication::translate("main", "Source file to encrypt or decrypt."));

        QCommandLineOption encryptOption("e", QCoreApplication::translate("main", "Encrypt the file"));
        parser.addOption(encryptOption);

        QCommandLineOption decryptOption("d", QCoreApplication::translate("main", "Decrypt the file"));
        parser.addOption(decryptOption);

        QCommandLineOption NameOption(QStringList() << "n" << "name"<< "name", QCoreApplication::translate("main", "The user name or e-mail."),QCoreApplication::translate("main", "name"));
        parser.addOption(NameOption);

        QCommandLineOption passphraseOption(QStringList() << "p" << "passphrase"<< "pass", QCoreApplication::translate("main", "The passphrase for encrypt or decrypt <source>."),QCoreApplication::translate("main", "passphrase"));
        parser.addOption(passphraseOption);

        // Process the actual command line arguments given by the user
        parser.process(app);

        const QStringList args = parser.positionalArguments();
        // source is args.at(0), destination is args.at(1)

        QString resultat;

        if (args.size() == 1 && parser.isSet(passphraseOption))
        {
            const auto targetFile = args.at(0);
            const auto passphrase = parser.value(passphraseOption);
            const auto enc        = parser.isSet(encryptOption);
            const auto dec        = parser.isSet(decryptOption);
            const auto name       = parser.value(NameOption);

            if (enc && dec)
            {
                cout << "ERROR: You must choose encryption OR decryption." << Qt::endl;
                return (0);
            }

            if (passphrase.size() < constants.MIN_PASS_LENGTH)
            {
                cout << "Passphrase must be minimum " + QString::number(constants.MIN_PASS_LENGTH) + "characters" << Qt::endl;
                return (0);
            }

            if (enc && name == "")
            {
                cout << "For encryption you must specify user name with -n option" << Qt::endl;
                return (0);
            }

            if (enc && name != "")
            {

                //resultat = encrypt(targetFile, passphrase, name);
                QStringList list;
                list.append(targetFile);
                Crypto_Thread Crypto;
                Crypto.setParam(true, list, passphrase,0, 0, true);
                Crypto.start();
                Crypto.wait();
                cout << "encrypted" << Qt::endl;
                return (0);
            }

            if (dec)
            {
                //resultat = decrypt(targetFile, passphrase);
                Crypto_Thread Crypto2;
                QStringList list2;
                list2.append(targetFile);
                Crypto2.setParam(false, list2, passphrase,0, 0, true);
                Crypto2.start();
                Crypto2.wait();
                cout << "decrypted" << Qt::endl;
                return (0);
            }
            cout << "Invalids or no arguments" << Qt::endl;
            return (0);
        }


            greeting(); // Print a ASCII Logo
            std::string direction;
            std::string filename;
            std::string password;

            cout << "Enter e for encrypt or d for decrypt :" << Qt::endl;
            std::getline(std::cin, direction);

            cout << "Enter filename :" << Qt::endl;
            std::getline(std::cin, filename);
            QString file(filename.c_str());

            cout << "Enter password :" << Qt::endl;
            std::getline(std::cin, password);
            QString pass(password.c_str());

            QStringList list;
            list.append(file);

            std::unique_ptr<Crypto_Thread> m_file_crypto;

             QObject::connect(
                m_file_crypto.get(), &Crypto_Thread::statusMessage,
                [=]( const QString &message ) { onMessageChanged(message); }
            );


            if (direction=="e")
            {
                m_file_crypto->setParam(true, list, pass,0, 0, true);
                m_file_crypto->start();
                m_file_crypto->wait();
                cout << "encrypted" << Qt::endl;
            }
            if (direction=="d")
            {
                m_file_crypto->setParam(false, list, pass,0, 0, true);
                m_file_crypto->start();
                m_file_crypto->wait();
                cout << "decrypted" << Qt::endl;
            }
            else
            {
                cout << "Invalid direction. e for encrypt, d for decrypt." << Qt::endl;
            }


            return 0;


}
