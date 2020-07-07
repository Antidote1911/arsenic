#ifndef TEXTCRYPTO_H
#define TEXTCRYPTO_H

#include <QObject>
#include <memory>

#include "consts.h"
#include "libexport.h"

class LIB_EXPORT textCrypto : public QObject {
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

    const std::unique_ptr<consts> m_const;

  signals:
};

#endif // TEXTCRYPTO_H
