#ifndef SKIN_H
#define SKKIN_H

#include <QObject>

class Skin : public QObject
{
    Q_OBJECT

public:
    Skin(QObject *parent = 0);

    QString getSkin(const QString &key);
    void setSkin(const QString &key);

private:


};

#endif // SKIN_H
