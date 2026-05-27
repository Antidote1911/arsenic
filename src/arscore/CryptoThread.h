#pragma once

#include <QObject>
#include <QStringList>
#include <QThread>
#include <atomic>

#include "botan_all.h"

class Crypto_Thread : public QThread {
    Q_OBJECT

  public:
    explicit Crypto_Thread(QObject *parent = nullptr);
    void run();

    // Signature inchangée pour ne pas modifier mainwindow.cpp ;
    // algo / argonmem / argoniter sont ignorés (format CPDF fixe).
    void setParam(bool direction,
                  QStringList const &filenames,
                  QString const &password,
                  QString const &algo,
                  quint32 const argonmem,
                  quint32 const argoniter,
                  bool const deletefile);

    void setChangePasswordParam(QStringList const &filenames,
                                QString const &oldPassword,
                                QString const &newPassword);

    void abort();

  signals:
    void updateProgress(const QString &path, quint32 percent);
    void statusMessage(const QString &message);
    void addEncrypted(const QString &inputFileName);
    void deletedAfterSuccess(const QString &inputFileName);

  private:
    // kek et kdfSalt sont gérés dans run() et transmis aux fonctions internes :
    //   - encrypt : KEK dérivé une seule fois pour toute la session
    //   - decrypt : KEK mis en cache pour éviter de relancer Argon2id si kdf_salt identique
    quint32 cpdfEncrypt(const QString &src_path,
                        const Botan::secure_vector<uint8_t> &kek,
                        const std::vector<uint8_t> &kdf_salt);

    quint32 cpdfDecrypt(const QString &src_path,
                        Botan::secure_vector<uint8_t> &kek,
                        std::vector<uint8_t> &cachedSalt);

    quint32 cpdfChangePassword(const QString &src_path);

    QStringList                  m_filenames;
    Botan::secure_vector<char>  m_password;
    Botan::secure_vector<char>  m_newPassword;
    bool                        m_direction{false};
    bool                        m_changePassword{false};
    bool              m_deletefile{false};
    std::atomic<bool> m_aborted{false};
};
