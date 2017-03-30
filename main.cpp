#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <iostream>
#include <QStyleFactory>
#include <Constants.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setOrganizationName(ARs::APP_ORGANIZATION);
    app.setOrganizationDomain(ARs::APP_URL);
    app.setApplicationName(ARs::APP_NAME);
    app.setApplicationVersion(ARs::APP_VERSION);

    // Addapt langage to system locale
    QString locale = QLocale::system().name().section('_', 0, 0);
    QTranslator translator;
    translator.load(QString("arsenic_") + locale);
    app.installTranslator(&translator);
    qApp->setStyle(QStyleFactory::create("Fusion"));

    // dark fusion good look on Windows
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53,53,53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25,25,25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53,53,53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53,53,53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));

    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    darkPalette.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);

    qApp->setPalette(darkPalette);

    qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");

    MainWindow w;
    w.show();
    return app.exec();
}
