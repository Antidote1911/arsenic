#pragma once

#include <QObject>
#include <QPointer>

class Skin : public QObject {
    Q_OBJECT

  public:
    Q_DISABLE_COPY(Skin)
    ~Skin() override;
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
