#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QStandardPaths>
#include <QTextStream>
#include <QCoreApplication>
#include <QDebug>

#include "loghtml.h"

logHtml::logHtml(QObject* parent)
    : QObject(parent)
{
}

logHtml::~logHtml()
{
}

QPointer<logHtml> logHtml::m_instance(nullptr);

void logHtml::clear()
{
    QFile logfile(getPath());
    logfile.remove();
}

QString logHtml::load()
{
    QFile logfile(getPath());

    if (!logfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "No log file in :" << getPath();
        return "";
    }

    QString out = logfile.readAll();
    return (out);
}

void logHtml::append(QString text)
{
    QFile logfile(getPath());
    logfile.open(QIODevice::ReadWrite | QIODevice::Text);
    QTextStream out(&logfile);
    out << text << Qt::endl;
    logfile.close();
}

QString logHtml::getPath()
{
    QString configPath;

#if defined(Q_OS_WIN)
    configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#elif defined(Q_OS_MACOS)
    configPath = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
#else
    configPath = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
#endif

    configPath += "/arsenic_log";

#ifdef QT_DEBUG
    configPath += "_debug";
#endif

    configPath += ".html";
    return (configPath);
}

logHtml* logHtml::instance()
{
    if (!m_instance) {
        m_instance = new logHtml(qApp);
    }

    return m_instance;
}
