#ifndef SKIN_H
#define SKIN_H

#include <QObject>

class Skin : public QObject {
    Q_OBJECT

public:
    Skin(QObject *parent = 0);

    void setSkin(const QString& key);

private:
};

#endif // SKIN_H
