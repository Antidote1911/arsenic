#pragma once

#include <QDebug>
#include <QObject>
#include "libexport.h"

class LIB_EXPORT Utils : public QObject {
    Q_OBJECT
  public:
    explicit Utils(QObject *parent = nullptr);
    static QString getFileSize(qint64 size);
    static qint64 getDirSize(const QString &curr_path);
    static void clearDir(const QString &dir_path);
    static QString getTempPath();
    static QString getBotanVersion();

    template <typename T>
    static void debugMessage(T test);

  signals:
};

template <typename T>
void Utils::debugMessage(T test)
{
    if constexpr (std::is_same_v<T, std::vector<QString>>) {
        for (auto const &e : test) {
            qDebug() << e;
        }
        return;
    }
    if constexpr (std::is_same_v<T, std::string>) {
        QString qstr = QString::fromStdString(test);
        qDebug() << qstr;
        return;
    }

    else {
        qDebug() << test;
    }
}
