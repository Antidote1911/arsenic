#ifndef UTILS_H
#define UTILS_H

#include <QDebug>
#include <QObject>

class Utils : public QObject {
    Q_OBJECT
  public:
    explicit Utils(QObject *parent = nullptr);
    static QString getFileSize(qint64 size);

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
#endif // UTILS_H
