#include "Config.h"
#include "fileCrypto.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QStyleFactory>
#include <QTranslator>
#include <QUnhandledException>
#include <QtGlobal>
#include <iostream>

#include "constants.h"

using namespace std;

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    app.setOrganizationName(ARs::APP_LONG_NAME);
    app.setOrganizationDomain(ARs::APP_URL);
    app.setApplicationName(ARs::APP_SHORT_NAME);
    app.setApplicationVersion(ARs::APP_VERSION.toString());
    app.setWindowIcon(QIcon(":/pixmaps/app.png"));
    app.setStyle("Fusion");

    QCommandLineParser parser;
    parser.setApplicationDescription(ARs::APP_DESCRIPTION);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("source", QCoreApplication::translate("main", "Source file to encrypt or decrypt."));

    QCommandLineOption encryptOption("e", QCoreApplication::translate("main", "Encrypt the file"));
    parser.addOption(encryptOption);

    QCommandLineOption decryptOption("d", QCoreApplication::translate("main", "Decrypt the file"));
    parser.addOption(decryptOption);

    QCommandLineOption passphraseOption(QStringList() << "p"
                                                      << "passphrase"
                                                      << "pass",
                                        QCoreApplication::translate("main", "The passphrase for encrypt or decrypt <source>."),
                                        QCoreApplication::translate("main", "passphrase"));
    parser.addOption(passphraseOption);

    // Process the actual command line arguments given by the user
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    // source is args.at(0), destination is args.at(1)

    QString resultat;
    Crypto_Thread Crypto;

    if ((args.size() == 1) && parser.isSet(passphraseOption)) {
        const auto targetFile = args.at(0);
        const auto passphrase = parser.value(passphraseOption);
        const auto enc        = parser.isSet(encryptOption);
        const auto dec        = parser.isSet(decryptOption);

        if (enc && dec) {
            cout << "ERROR: You must choose encryption OR decryption." << endl;
            return (0);
        }

        if (passphrase.size() < ARs::MIN_PASS_LENGTH) {
            auto tmp            = QString::number(ARs::MIN_PASS_LENGTH);
            std::string minimum = tmp.toUtf8().constData();
            cout << "Passphrase must be minimum " + minimum + " characters" << endl;
            return (0);
        }

        if (enc) {
            QStringList listFiles;
            listFiles.append(targetFile);
            Crypto.setParam(true, listFiles, passphrase, config()->get("CRYPTO/argonMemory").toInt(), config()->get("CRYPTO/argonItr").toInt(), false);
            Crypto.start();
            Crypto.wait();
            return (0);
        }

        if (dec) {
            QStringList listFiles;
            listFiles.append(targetFile);
            Crypto.setParam(false, listFiles, passphrase, config()->get("CRYPTO/argonMemory").toInt(), config()->get("CRYPTO/argonItr").toInt(), false);
            Crypto.start();
            Crypto.wait();
            return (0);
        }

        cout << "Invalids or no arguments" << endl;
        return (0);
    }
    else {
        MainWindow w;
        w.session();
        return (app.exec());
    }
}
