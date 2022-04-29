#include "mainclass.h"
#include <QDebug>
#include <QStringList>
#include <iostream>

using namespace std;

MainClass::MainClass(QObject *parent)
    : QObject(parent)
{
    // get the instance of the main application
    app = QCoreApplication::instance();
    app->setOrganizationName(m_const->APP_LONG_NAME);
    app->setOrganizationDomain(m_const->APP_URL);
    app->setApplicationName(m_const->APP_SHORT_NAME);
    app->setApplicationVersion(m_const->APP_VERSION.toString());

    QObject::connect(m_crypto.get(), &Triple_Crypto_Thread::statusMessage,
                    this, [=](const QString &message) { onMessageChanged(message); });

    QObject::connect(m_crypto.get(), &Triple_Crypto_Thread::updateProgress,
                    this, [=](const QString &path, quint32 percent) { displayProgress(path, percent); });

    // setup everything here
    // create any global objects
    // setup debug and warning mode
}

// 10ms after the application starts this method will run
// all QT messaging is running at this point so threads, signals and slots
// will all work as expected.
void MainClass::run()
{
    // Add your main code here
    // you must call quit when complete or the program will stay in the
    // messaging loop
    // qDebug() << "MainClass.Run is executing";
    greetings();
    QCommandLineParser parser;
    parser.setApplicationDescription(m_const->APP_DESCRIPTION);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("source", QCoreApplication::translate("main", "Source file to encrypt or decrypt."));

    QCommandLineOption passphraseOption(QStringList() << "p"
                                                      << "passphrase"
                                                      << "pass",
                                        QCoreApplication::translate("main", "The passphrase for encrypt or decrypt <source>."), QCoreApplication::translate("main", "passphrase"));
    parser.addOption(passphraseOption);

    QCommandLineOption directionOption(QStringList() << "d"
                                                     << "direction",
                                       QCoreApplication::translate("main", "ENCRYPT or DECRYPT <source>."), QCoreApplication::translate("main", "direction"));
    parser.addOption(directionOption);

    // Process the actual command line arguments given by the user
    parser.process(*app);

    const QStringList args = parser.positionalArguments();
    // source is args.at(0)

    if (args.size() == 1 && parser.isSet(passphraseOption) && parser.isSet(directionOption)) {
        const auto targetFile = args.at(0);
        const auto passphrase = parser.value(passphraseOption);
        const auto direction  = parser.value(directionOption);

        if (direction != "ENCRYPT" && direction != "DECRYPT") {
            cout << "ERROR: INVALID DIRECTION" << endl;
            cout << "You must choose encryption OR decryption" << endl;
            cout << "with -d ENCRYPT or -d DECRYPT" << endl;
            quit();
        }

        if (passphrase.size() < m_const->MIN_PASS_LENGTH) {
            cout << "Passphrase must be minimum 8 characters" << endl;
            quit();
        }
        QStringList list;
        list.append(targetFile);

        if (direction == "ENCRYPT") {
            m_crypto->setParam(true, list, passphrase, 1, 1, false);
            m_crypto->start();
            m_crypto->wait();
            quit();
        }

        if (direction == "DECRYPT") {
            m_crypto->setParam(false, list, passphrase, 1, 1, false);
            m_crypto->start();
            m_crypto->wait();
            quit();
        }

        quit();
    }
    else {
        parser.showHelp();
    }

    quit();
}
void MainClass::greetings()
{
    string breakLine = "############################################\n";
    cout << breakLine;
    cout << m_const->APP_DESCRIPTION.toStdString() + " version " + m_const->APP_VERSION.toString().toStdString() << endl;
    cout << breakLine << endl;
}

// call this routine to quit the application
void MainClass::quit()
{
    // you can do some cleanup here
    // then do emit finished to signal CoreApplication to quit
    emit finished();
}

// shortly after quit is called the CoreApplication will signal this routine
// this is a good place to delete any objects that were created in the
// constructor and/or to stop any threads
void MainClass::aboutToQuitApp()
{
    // stop threads
    // sleep(1);   // wait for threads to stop.
    // delete any objects
}

void MainClass::onMessageChanged(QString message)
{
    cout << message.toStdString() << endl;
}
void MainClass::displayProgress(const QString &path, quint32 percent)
{
    bar.progress(percent, 100);
}
