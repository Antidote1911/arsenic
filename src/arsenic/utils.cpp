#include "utils.h"
#include <cmath>
#include <vector>
#include <QDir>

Utils::Utils(QObject *parent)
    : QObject(parent)
{
}

QString Utils::getTempPath()
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
    userPath += "temp/";
#else
    userPath += "temp/";
#endif
    return (userPath);
}

void Utils::clearDir(const QString &dir_path)
{
    QDir qd(dir_path);

    if (qd.exists()) {
        QList<QFileInfo> item_list = qd.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoSymLinks |
                                                      QDir::NoDotAndDotDot | QDir::Hidden);

        for (auto it = item_list.begin(); it != item_list.end(); ++it) {
            if (it->isDir())
                QDir(it->absoluteFilePath()).removeRecursively();
            else if (it->isFile())
                QDir().remove(it->absoluteFilePath());
        }
    }
}

qint64 Utils::getDirSize(const QString &curr_dir)
{
    qint64 size = 0;

    // get a list of elements in the current directory

    QDir qdir             = QDir(curr_dir);
    QList<QFileInfo> list = qdir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoSymLinks |
                                               QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    for (int i = 0; i < list.size(); i++) {
        QFileInfo file = list.at(i);

        // otherwise, continue parsing files and directories
        if (file.isFile())
            size += file.size();
        else if (file.isDir())
            size += getDirSize(file.absoluteFilePath());
    }

    return size;
}

QString Utils::getFileSize(qint64 size)
{
    static const double KiB = pow(2, 10);
    static const double MiB = pow(2, 20);
    static const double GiB = pow(2, 30);
    static const double TiB = pow(2, 40);
    static const double PiB = pow(2, 50);
    // convert to appropriate units based on the size of the item
    if (size >= 0) {
        static const int precision = 0;
        if (size < KiB) {
            return (QString::number(size, 'f', precision) + " B");
        }
        else if (size < MiB) {
            return (QString::number(size / KiB, 'f', precision) + " KiB");
        }
        else if (size < GiB) {
            return (QString::number(size / MiB, 'f', precision) + " MiB");
        }
        else if (size < TiB) {
            return (QString::number(size / GiB, 'f', precision) + " GiB");
        }
        else if (size < PiB) {
            return (QString::number(size / TiB, 'f', precision) + " TiB");
        }
        else {
            return (QString::number(size / PiB, 'f', precision) + " PiB");
        }
    }
    else {
        return ("");
    }
}
