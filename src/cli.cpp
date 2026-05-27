#include "cli.h"
#include "arscore/CryptoThread.h"
#include "arscore/consts.h"
#include "arscore/messages.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>

#include <iostream>

// ── Détection des messages d'erreur (issus de errorCodeToString) ──────────────

static bool isErrorMessage(const QString& msg)
{
    return msg.contains("Failure",         Qt::CaseInsensitive)
        || msg.contains("not found",       Qt::CaseInsensitive)
        || msg.contains("could not be",    Qt::CaseInsensitive)
        || msg.contains("already exists",  Qt::CaseInsensitive)
        || msg.contains("not an Arsenic",  Qt::CaseInsensitive)
        || msg.contains("MAC verification",Qt::CaseInsensitive)
        || msg.contains("Wrong old",       Qt::CaseInsensitive)
        || msg.contains("Can't read",      Qt::CaseInsensitive)
        || msg.contains("Aborted",         Qt::CaseInsensitive);
}

// ── Point d'entrée CLI ────────────────────────────────────────────────────────

int runCli(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(consts::APP_SHORT_NAME);
    app.setApplicationVersion(consts::APP_VERSION.toString());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Arsenic – Strong file encryption\n\n"
        "Examples:\n"
        "  arsenic -e -p mypassword file.txt\n"
        "  arsenic -d -p mypassword file.txt.cpdf\n"
        "  arsenic --change-password --old oldpwd --new newpwd file.cpdf");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption encOpt({"e", "encrypt"},
        "Encrypt the given files.");
    QCommandLineOption decOpt({"d", "decrypt"},
        "Decrypt the given files.");
    QCommandLineOption pwdOpt({"p", "password"},
        "Password used for encryption or decryption.", "password");
    QCommandLineOption delOpt({"x", "delete"},
        "Delete the source file after a successful operation.");
    QCommandLineOption chgOpt("change-password",
        "Change the password of already-encrypted files.");
    QCommandLineOption oldOpt("old",
        "Old password (required with --change-password).", "password");
    QCommandLineOption newOpt("new",
        "New password (required with --change-password).", "password");

    parser.addOption(encOpt);
    parser.addOption(decOpt);
    parser.addOption(pwdOpt);
    parser.addOption(delOpt);
    parser.addOption(chgOpt);
    parser.addOption(oldOpt);
    parser.addOption(newOpt);
    parser.addPositionalArgument("files", "Files to process.", "<file>...");

    parser.process(app);

    const bool doEnc = parser.isSet(encOpt);
    const bool doDec = parser.isSet(decOpt);
    const bool doChg = parser.isSet(chgOpt);

    const int modeCount = (doEnc ? 1 : 0) + (doDec ? 1 : 0) + (doChg ? 1 : 0);
    if (modeCount != 1) {
        std::cerr << "Error: specify exactly one of -e/--encrypt, -d/--decrypt, --change-password\n";
        return 1;
    }

    const QStringList files = parser.positionalArguments();
    if (files.isEmpty()) {
        std::cerr << "Error: no input files specified.\n";
        parser.showHelp(1);
    }

    Crypto_Thread ct;

    if (doChg) {
        if (!parser.isSet(oldOpt) || !parser.isSet(newOpt)) {
            std::cerr << "Error: --change-password requires --old and --new.\n";
            return 1;
        }
        ct.setChangePasswordParam(files, parser.value(oldOpt), parser.value(newOpt));
    } else {
        if (!parser.isSet(pwdOpt)) {
            std::cerr << "Error: -p/--password is required.\n";
            return 1;
        }
        ct.setParam(doEnc, files, parser.value(pwdOpt), {}, 0, 0, parser.isSet(delOpt));
    }

    bool anyError = false;

    QObject::connect(&ct, &Crypto_Thread::statusMessage,
        [&anyError](const QString& msg) {
            if (msg.isEmpty()) return;
            if (isErrorMessage(msg)) {
                std::cerr << msg.toStdString() << "\n";
                anyError = true;
            } else {
                std::cout << msg.toStdString() << "\n" << std::flush;
            }
        });

    QString currentPath;
    QObject::connect(&ct, &Crypto_Thread::updateProgress,
        [&currentPath](const QString& path, quint32 pct) {
            if (currentPath != path) {
                if (!currentPath.isEmpty()) std::cout << "\n";
                currentPath = path;
            }
            std::cout << "\r  " << pct << "%  " << std::flush;
            if (pct == 100) {
                std::cout << "\n";
                currentPath.clear();
            }
        });

    QEventLoop loop;
    QObject::connect(&ct, &QThread::finished, &loop, &QEventLoop::quit);
    ct.start();
    loop.exec();

    return anyError ? 1 : 0;
}
