#pragma once

#include <QObject>
#include <QPointer>

class Skin : public QObject {
    Q_OBJECT

  public:
    Q_DISABLE_COPY(Skin)
    ~Skin() override;
    QString getSkin(const QString &key);
    void setSkin(const QString &key);
    static Skin *instance();

  private:
    explicit Skin(QObject *parent = nullptr);
    static QPointer<Skin> m_instance;
};

inline Skin *skin()
{
    return Skin::instance();
}
