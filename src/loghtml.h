#pragma once

#include <QObject>
#include <QPointer>

class logHtml : public QObject {
    Q_OBJECT

  public:
    Q_DISABLE_COPY(logHtml)
    ~logHtml() override;
    QString load();
    void append(QString text);
    void clear();
    QString getPath();

    static logHtml* instance();

  private:
    explicit logHtml(QObject* parent = nullptr);

    static QPointer<logHtml> m_instance;

  signals:
};

inline logHtml* loghtml()
{
    return logHtml::instance();
}
