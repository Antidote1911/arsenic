#include "utils.h"
#include <cmath>
#include <vector>

Utils::Utils(QObject *parent)
    : QObject(parent)
{
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
