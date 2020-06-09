#ifndef UTILS_H
#define UTILS_H

#include <QDebug>
#include <QObject>
#include <iostream>

class Utils : public QObject
{
    Q_OBJECT
public:
    explicit Utils(QObject *parent = nullptr);
    static QString getFileSize(qint64 size);
    template <typename T> static void afficher(std::vector<T> const &v);

signals:
};

// Version générique
template <typename T> void Utils::afficher(std::vector<T> const &v)
{
    if constexpr (std::is_same_v<T, const QString>) {
        for (auto const &e : v) {
            qDebug() << QString::number(e);
        }
    } else {
        for (auto const &e : v) {
            qDebug() << e;
        }
    }
}

#endif // UTILS_H
