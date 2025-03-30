#pragma once

#include <QObject>
#include <memory>

#include "consts.h"

class textCrypto : public QObject {
    Q_OBJECT
  public:
    explicit textCrypto(QObject *parent = nullptr);

    void start(const QString &password, bool dirrection);
    quint32 finish(QString &text);

  private:
    quint32 encryptString(QString &plaintext, const QString &password);

    quint32 decryptString(QString &ciphertext, const QString &password);

    QString m_password;
    bool m_dirrection;

    template<typename T> inline constexpr uint8_t get_byte_var(size_t byte_num, T input)
       {
       return static_cast<uint8_t>(
          input >> (((~byte_num)&(sizeof(T)-1)) << 3)
          );
       }

    const std::unique_ptr<consts> m_const;

  signals:
};

