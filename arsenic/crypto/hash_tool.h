#ifndef HASH_TOOL_H
#define HASH_TOOL_H
#include <QThread>
#include <QString>


class Hash_Tool : public QThread
{
    Q_OBJECT
public:
    explicit Hash_Tool(QObject *parent = 0);
    void run();
    bool aborted;
    void setParam(QStringList filenames, QString algo);


signals:
    void PercentProgressHashChanged(QString, qint64, QString);
    void statusResult(QString, QString, QString);

private:
    QStringList m_filenames;
    QString m_algo;

    void calculateHash(QStringList& filename, QString nameHash);

public slots:


};

#endif // HASH_TOOL_H
