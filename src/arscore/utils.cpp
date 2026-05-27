#include "utils.h"
#include <QDir>
#include <QStandardPaths>
#include <QStringBuilder>

#include "messages.h"

Utils::Utils(QObject *parent)
    : QObject(parent)
{
}

QString Utils::getTempPath()
{
    QString configPath;

#if defined(Q_OS_WIN)
    configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#elif defined(Q_OS_MACOS)
    configPath = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
#else
    configPath = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
#endif

    configPath += "/temp/";
    return (configPath);
}

void Utils::clearDir(const QString &dir_path)
{
    QDir qd(dir_path);

    if (qd.exists()) {
        const QList<QFileInfo> item_list = qd.entryInfoList(QDir::Dirs | QDir::Files |
                                                             QDir::NoSymLinks | QDir::NoDotAndDotDot |
                                                             QDir::Hidden);
        for (const auto& item : item_list) {
            if (item.isDir())
                QDir(item.absoluteFilePath()).removeRecursively();
            else if (item.isFile())
                QDir().remove(item.absoluteFilePath());
        }
    }
}

qint64 Utils::getDirSize(const QString &curr_dir)
{
    qint64 size = 0;

    const QDir qdir(curr_dir);
    const QList<QFileInfo> list = qdir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoSymLinks |
                                                     QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const auto& file : list) {
        if (file.isFile())
            size += file.size();
        else if (file.isDir())
            size += getDirSize(file.absoluteFilePath());
    }

    return size;
}

QString Utils::getFileSize(qint64 size)
{
    static constexpr double KiB = 1024.0;
    static constexpr double MiB = 1024.0 * 1024.0;
    static constexpr double GiB = 1024.0 * 1024.0 * 1024.0;
    static constexpr double TiB = 1024.0 * 1024.0 * 1024.0 * 1024.0;
    static constexpr double PiB = 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
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

QString Utils::uniqueFileName(const QString &fileName)
{
    QFileInfo originalFile(fileName);
    auto uniqueFileName      = fileName;
    auto foundUniqueFileName = false;
    auto i                   = 0;
    while (!foundUniqueFileName && i < 100000) {
        QFileInfo uniqueFile(uniqueFileName);
        if (uniqueFile.exists() && uniqueFile.isFile()) // Write number of copies before file extension
        {
            uniqueFileName = originalFile.absolutePath() % QDir::separator() % originalFile.baseName() % QString{" (%1)"}.arg(i + 2);

            if (!originalFile.completeSuffix().isEmpty()) // Add the file extension if there is one
            {
                uniqueFileName += QStringLiteral(".") % originalFile.completeSuffix();
            }
            ++i;
        }
        else {
            foundUniqueFileName = true;
        }
    }
    return (uniqueFileName);
}

