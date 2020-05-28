#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QStandardPaths>

#include "loghtml.h"

logHtml::logHtml(QObject *parent) : QObject(parent)
{
}

void logHtml::clear()
{
    QFile logfile(getPath());
    logfile.remove();
}

QString logHtml::load()
{
    QFile logfile(getPath());
    logfile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString out = logfile.readAll();
    return(out);
}

void logHtml::append(QString text)
{
    QFile logfile(getPath());
    logfile.open(QIODevice::ReadWrite | QIODevice::Text);
    QTextStream out(&logfile);
    out << text << endl;
    logfile.close();
}

QString logHtml::getPath()
{
    QString userPath;
    QString homePath = QDir::homePath();

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // we can't use QStandardPaths on X11 as it uses XDG_DATA_HOME instead of XDG_CONFIG_HOME
    QByteArray env = qgetenv("XDG_CONFIG_HOME");

    if (env.isEmpty()) {
        userPath = homePath;
        userPath += "/.config";
    }
    else if (env[0] == '/') {
        userPath = QFile::decodeName(env);
    }
    else {
        userPath = homePath;
        userPath += '/';
        userPath += QFile::decodeName(env);
    }

    userPath += "/arsenic/";
#else
    userPath = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    // storageLocation() appends the application name ("/arsenic") to the end
    userPath += "/";
#endif

#ifdef QT_DEBUG
    userPath += "arsenic_log_debug.html";
#else
    userPath += "arsenic_log.html";
#endif
    return(userPath);
}
