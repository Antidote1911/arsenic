#include "mainwindow.h"
#include <QApplication>
#include <QtGlobal>
#include <QUnhandledException>
#include <QStyleFactory>
#include <QDebug>
#include <QCommandLineParser>
#include <QTranslator>
#include "encrypt.h"
#include "decrypt.h"

#include "constants.h"


int main(int argc, char *argv[])
{
    QTextStream cin(stdin);
    QTextStream cout(stdout);

    QApplication app(argc, argv);
    app.setOrganizationName(ARs::APP_LONG_NAME);
    app.setOrganizationDomain(ARs::APP_URL);
    app.setApplicationName(ARs::APP_SHORT_NAME);
    app.setApplicationVersion(ARs::APP_VERSION);
    app.setWindowIcon(QIcon(":/pixmaps/icon.png"));
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
            cout << "ERROR: You must choose encryption OR decryption." << endl;
            return (0);
        }

        if (passphrase.size() < ARs::MIN_PASS_LENGTH)
        {
            cout << "Passphrase must be minimum " + QString::number(ARs::MIN_PASS_LENGTH) + "characters" << endl;
            return (0);
        }

        if (enc && name == "")
        {
            cout << "For encryption you must specify user name with -n option" << endl;
            return (0);
        }

        if (enc && name != "")
        {

            resultat = encrypt(targetFile, passphrase, name);
            cout << endl << resultat << endl;
            return (0);
        }

        if (dec)
        {
            resultat = decrypt(targetFile, passphrase);
            cout << endl << resultat << endl;
            return (0);
        }
        cout << "Invalids or no arguments" << endl;
        return (0);
    }
    else
    {


        MainWindow w;
        w.session();
        return app.exec();
    }




}
