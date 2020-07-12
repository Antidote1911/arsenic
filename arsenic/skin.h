#pragma once

#include <QObject>

class Skin : public QObject {
    Q_OBJECT

  public:
    Skin(QObject *parent = 0);

    void setSkin(const QString &key);

  private:
};
