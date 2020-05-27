#ifndef LOGHTML_H
#define LOGHTML_H

#include <QObject>

class logHtml : public QObject
{
    Q_OBJECT
public:
    explicit logHtml(QObject *parent = nullptr);
    QString load();
    void append(QString text);
    void clear();

private:
    QString getPath();


signals:
};

#endif // LOGHTML_H
