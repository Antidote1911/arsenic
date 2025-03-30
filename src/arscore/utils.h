#pragma once

#include <QDebug>
#include <QObject>
#include <QFile>

class Utils : public QObject {
    Q_OBJECT
  public:
    explicit Utils(QObject *parent = nullptr);
    static QString getFileSize(qint64 size);
    static qint64 getDirSize(const QString &curr_path);
    static void clearDir(const QString &dir_path);
    static QString getTempPath();
    static QString uniqueFileName(const QString &fileName);
    static QString getAlgo(QFile &file);

  signals:
};
