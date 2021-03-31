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
    static QString uniqueFileName(const QString &fileName);

  signals:
};
