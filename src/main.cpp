#include "Config.h"
#include "mainwindow.h"
#include "Translator.h"
#include "cli.h"
#include <QApplication>
#include <QDebug>
#include <QStyleFactory>
#include <QTranslator>
#include <QProcess>

#include "arscore/consts.h"

#include <ranges>
#include <span>
#include <string_view>

int main(int argc, char *argv[])
{
    // Détecter le mode CLI avant de créer QApplication (qui initialise le GUI)
    using namespace std::string_view_literals;
    const auto isCliFlag = [](const char* a) noexcept {
        const std::string_view sv{a};
        return sv == "-e"sv || sv == "--encrypt"sv
            || sv == "-d"sv || sv == "--decrypt"sv
            || sv == "--change-password"sv;
    };
    if (std::ranges::any_of(std::span(argv + 1, argc - 1), isCliFlag))
        return runCli(argc, argv);

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
