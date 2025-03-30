#include "utils.h"
#include <cmath>
#include <vector>
#include <QDir>
#include <QStandardPaths>
#include <QDataStream>
#include <QVersionNumber>
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

QString Utils::getAlgo(QFile &file)
{

    // open the source file and extract all informations necessary for decryption
    QDataStream src_stream(&file);
    src_stream.setVersion(QDataStream::Qt_5_0);

    // Read and check the header
    quint32 magic;
    src_stream >> magic;

    // Read and check the version
    QVersionNumber version;
    src_stream >> version;

    // Read Argon2 parameters
    quint32 memlimit;
    src_stream >> memlimit;

    quint32 iterations;
    src_stream >> iterations;

    QString algo;
    src_stream >> algo;

    qint64 fileNameSize;
    src_stream >> fileNameSize;


    return (algo);
}
