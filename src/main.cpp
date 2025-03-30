#include "Config.h"
#include "mainwindow.h"
#include "Translator.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QStyleFactory>
#include <QTranslator>
#include <QUnhandledException>
#include <QtGlobal>
#include <QProcess>
#include <iostream>
#include <QWidget>

#include "arscore/consts.h"

using namespace std;

void handler(QString val)
{
    std::cout << val.toStdString() << std::endl;
}

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    // don't set organizationName as that changes the return value of
    // QStandardPaths::writableLocation(QDesktopServices::DataLocation)

    app.setOrganizationDomain(consts::APP_URL);
    app.setApplicationName(consts::APP_SHORT_NAME);
    app.setApplicationVersion(consts::APP_VERSION.toString());
    app.setWindowIcon(QIcon(":/pixmaps/app.png"));
    app.setStyle(QStyleFactory::create("Fusion"));
    qDebug() << QStyleFactory::keys();
    Translator::installTranslators();

    MainWindow w;
    w.show();
    int currentExitCode = app.exec();
    if (currentExitCode == consts::EXIT_CODE_REBOOT) {
        QProcess::startDetached(qApp->applicationFilePath(), QStringList());
        return 0;
    }
    return currentExitCode;
}
