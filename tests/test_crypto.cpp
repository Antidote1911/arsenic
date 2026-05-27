#include <QtTest>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>

#include "../src/arscore/CryptoThread.h"
#include "../src/arscore/messages.h"

// ── Runner synchrone ──────────────────────────────────────────────────────────
// Wraps Crypto_Thread : configure, lance, attend la fin via QEventLoop,
// retourne les messages émis par statusMessage.

struct RunResult {
    QStringList messages;

    bool contains(quint32 code) const {
        return messages.contains(errorCodeToString(code));
    }
    bool encryptOk()       const { return contains(CRYPT_SUCCESS);           }
    bool decryptOk()       const { return contains(DECRYPT_SUCCESS);         }
    bool changeOk()        const { return contains(CHANGE_PASSWORD_SUCCESS); }
    bool decryptFailed()   const { return contains(DECRYPT_FAIL);            }
    bool wrongOldPwd()     const { return contains(WRONG_OLD_PASSWORD);      }
    bool macFailed()       const { return contains(GLOBAL_MAC_FAIL);         }
    bool notArsenicFile()  const { return contains(NOT_AN_ARSENIC_FILE);     }
};

static RunResult runEncrypt(const QStringList& files,
                             const QString& password,
                             bool del = false)
{
    Crypto_Thread ct;
    ct.setParam(true, files, password, {}, 0, 0, del);
    RunResult r;
    QObject::connect(&ct, &Crypto_Thread::statusMessage, [&](const QString& m) {
        if (!m.isEmpty()) r.messages << m;
    });
    QEventLoop loop;
    QObject::connect(&ct, &QThread::finished, &loop, &QEventLoop::quit);
    ct.start();
    loop.exec();
    return r;
}

static RunResult runDecrypt(const QStringList& files,
                             const QString& password,
                             bool del = false)
{
    Crypto_Thread ct;
    ct.setParam(false, files, password, {}, 0, 0, del);
    RunResult r;
    QObject::connect(&ct, &Crypto_Thread::statusMessage, [&](const QString& m) {
        if (!m.isEmpty()) r.messages << m;
    });
    QEventLoop loop;
    QObject::connect(&ct, &QThread::finished, &loop, &QEventLoop::quit);
    ct.start();
    loop.exec();
    return r;
}

static RunResult runChangePassword(const QStringList& files,
                                    const QString& oldPwd,
                                    const QString& newPwd)
{
    Crypto_Thread ct;
    ct.setChangePasswordParam(files, oldPwd, newPwd);
    RunResult r;
    QObject::connect(&ct, &Crypto_Thread::statusMessage, [&](const QString& m) {
        if (!m.isEmpty()) r.messages << m;
    });
    QEventLoop loop;
    QObject::connect(&ct, &QThread::finished, &loop, &QEventLoop::quit);
    ct.start();
    loop.exec();
    return r;
}

// Flip un octet à `offset` (négatif = depuis la fin) dans le fichier donné.
static bool tamperFile(const QString& path, int offset, uint8_t xorMask = 0xFF)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) return false;
    const qint64 sz  = f.size();
    const qint64 pos = (offset >= 0) ? offset : sz + offset;
    if (pos < 0 || pos >= sz) return false;
    if (!f.seek(pos)) return false;
    char byte;
    if (f.read(&byte, 1) != 1) return false;
    byte = static_cast<char>(static_cast<uint8_t>(byte) ^ xorMask);
    if (!f.seek(pos)) return false;
    f.write(&byte, 1);
    return true;
}

// ── Suite de tests ────────────────────────────────────────────────────────────

class TestArsenic : public QObject {
    Q_OBJECT

private slots:

    // ── 1. Roundtrip basique ─────────────────────────────────────────────────

    void roundtrip_basic()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString src  = dir.filePath("hello.txt");
        const QString cpdf = src + ".cpdf";
        const QByteArray plain("Hello, Arsenic!");

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(plain); f.close();

        QVERIFY(runEncrypt({src}, "mdp_test").encryptOk());
        QVERIFY(QFile::exists(cpdf));
        QVERIFY(QFile::exists(src)); // source intacte (pas de -x)

        QVERIFY(runDecrypt({cpdf}, "mdp_test").decryptOk());

        // Le fichier déchiffré doit avoir le même contenu
        QFile out(src + ".1"); // si src existe, Utils::uniqueFileName ajoute " (2)" ou renomme
        // Le fichier récupéré a le même nom que l'original (src = hello.txt)
        // QFile::exists(src) suffit car cryptopad/arsenic écrase ou renomme uniquement
        // si le fichier existe déjà. Ici src existe → uniqueFileName le renomme.
        // On trouve le résultat en cherchant tous les .txt dans le répertoire.
        bool found = false;
        for (const auto& fi : QDir(dir.path()).entryInfoList({"*.txt"}, QDir::Files)) {
            QFile r(fi.filePath());
            if (r.open(QIODevice::ReadOnly) && r.readAll() == plain) { found = true; break; }
        }
        QVERIFY2(found, "Le contenu déchiffré ne correspond pas au plaintext original");
    }

    // ── 2. Roundtrip multi-chunk (> 1 MiB) ──────────────────────────────────

    void roundtrip_multiChunk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // 3 MiB de données (3 chunks de 1 MiB)
        const QByteArray plain(3 * (1 << 20), 'Z');
        const QString src  = dir.filePath("big.bin");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(plain); f.close();

        QVERIFY(runEncrypt({src}, "pwd_chunk").encryptOk());
        QVERIFY(QFile::exists(cpdf));

        QVERIFY(runDecrypt({cpdf}, "pwd_chunk").decryptOk());

        bool found = false;
        for (const auto& fi : QDir(dir.path()).entryInfoList({"*.bin"}, QDir::Files)) {
            QFile r(fi.filePath());
            if (r.open(QIODevice::ReadOnly) && r.readAll() == plain) { found = true; break; }
        }
        QVERIFY2(found, "Roundtrip multi-chunk échoué");
    }

    // ── 3. Fichier vide ───────────────────────────────────────────────────────

    void roundtrip_emptyFile()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("empty.dat");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.close();

        QVERIFY(runEncrypt({src}, "pwd").encryptOk());
        QVERIFY(runDecrypt({cpdf}, "pwd").decryptOk());
    }

    // ── 4. Mauvais mot de passe ──────────────────────────────────────────────

    void wrongPassword()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("secret.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("secret"); f.close();
        QVERIFY(runEncrypt({src}, "bon_mdp").encryptOk());

        const auto r = runDecrypt({cpdf}, "mauvais_mdp");
        QVERIFY2(r.decryptFailed(), "Un mauvais mot de passe aurait dû échouer");
    }

    // ── 5. Changement de mot de passe ────────────────────────────────────────

    void changePassword_success()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("doc.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("contenu"); f.close();
        QVERIFY(runEncrypt({src}, "ancien").encryptOk());

        QVERIFY(runChangePassword({cpdf}, "ancien", "nouveau").changeOk());

        // Déchiffrement avec le nouveau mot de passe → succès
        QVERIFY2(runDecrypt({cpdf}, "nouveau").decryptOk(),
                 "Déchiffrement avec le nouveau mot de passe devrait réussir");
    }

    // ── 6. Ancien mot de passe invalide après changement ────────────────────

    void changePassword_oldInvalidAfter()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("doc.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("data"); f.close();
        QVERIFY(runEncrypt({src}, "ancien").encryptOk());
        QVERIFY(runChangePassword({cpdf}, "ancien", "nouveau").changeOk());

        const auto r = runDecrypt({cpdf}, "ancien");
        QVERIFY2(r.decryptFailed(),
                 "L'ancien mot de passe ne doit plus fonctionner après le changement");
    }

    // ── 7. Mauvais ancien mot de passe lors du changement ───────────────────

    void changePassword_wrongOld()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("doc.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("data"); f.close();
        QVERIFY(runEncrypt({src}, "bon").encryptOk());

        const auto r = runChangePassword({cpdf}, "mauvais", "nouveau");
        QVERIFY2(r.wrongOldPwd(),
                 "Un mauvais ancien mot de passe devrait être rejeté");
    }

    // ── 8. Altération du trailer MAC ────────────────────────────────────────

    void tampered_macTrailer()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("file.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(64, 'A')); f.close();
        QVERIFY(runEncrypt({src}, "pw").encryptOk());

        // Dernier octet = dernier octet du HMAC-SHA256 trailer
        QVERIFY(tamperFile(cpdf, -1));

        const auto r = runDecrypt({cpdf}, "pw");
        QVERIFY2(r.macFailed() || r.decryptFailed(),
                 "Une altération du trailer MAC devrait être détectée");
    }

    // ── 9. Altération dans la zone des chunks ────────────────────────────────
    // Offset 309 = header(257) + nonces(52) = premier octet du ciphertext interne

    void tampered_chunkData()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("file.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(256, 'B')); f.close();
        QVERIFY(runEncrypt({src}, "pw").encryptOk());

        // 257 (header) + 52 (nonces) = premier octet du ciphertext AES-SIV
        QVERIFY(tamperFile(cpdf, 257 + 52));

        const auto r = runDecrypt({cpdf}, "pw");
        QVERIFY2(r.macFailed() || r.decryptFailed(),
                 "Une altération dans les données chiffrées devrait être détectée");
    }

    // ── 10. Altération du sel KDF ────────────────────────────────────────────
    // Offset 21 = juste après magic(4)+version(1)+file_id(16)

    void tampered_kdfSalt()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("file.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("test"); f.close();
        QVERIFY(runEncrypt({src}, "pw").encryptOk());

        QVERIFY(tamperFile(cpdf, 21)); // kdf_salt[0]

        const auto r = runDecrypt({cpdf}, "pw");
        // Sel altéré → KEK différent → unwrapDEK échoue → DECRYPT_FAIL
        QVERIFY2(r.decryptFailed() || r.macFailed(),
                 "Une altération du sel KDF devrait être détectée");
    }

    // ── 11. Fichier non-Arsenic ───────────────────────────────────────────────

    void notAnArsenicFile()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString path = dir.filePath("random.bin");

        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(512, '\x42')); f.close();

        const auto r = runDecrypt({path}, "pw");
        QVERIFY2(r.notArsenicFile(),
                 "Un fichier aléatoire devrait être rejeté comme non-Arsenic");
    }

    // ── 12. Suppression de la source après chiffrement ──────────────────────

    void deleteAfterEncrypt()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("todel.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("data"); f.close();
        QVERIFY(runEncrypt({src}, "pw", /*del=*/true).encryptOk());

        QVERIFY2(!QFile::exists(src),
                 "La source devrait être supprimée après chiffrement avec --delete");
        QVERIFY2(QFile::exists(cpdf), "Le fichier chiffré devrait exister");
    }

    // ── 13. Suppression de la source après déchiffrement ────────────────────

    void deleteAfterDecrypt()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("todel.txt");
        const QString cpdf = src + ".cpdf";

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("data"); f.close();
        QVERIFY(runEncrypt({src}, "pw").encryptOk());
        QVERIFY(runDecrypt({cpdf}, "pw", /*del=*/true).decryptOk());

        QVERIFY2(!QFile::exists(cpdf),
                 "Le fichier .cpdf devrait être supprimé après déchiffrement avec --delete");
    }

    // ── 14. Non-déterminisme ─────────────────────────────────────────────────
    // Deux chiffrements du même fichier → .cpdf différents (file_id + nonces aléatoires)

    void nonDeterministic()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());

        auto makeAndEncrypt = [&](const QString& name) -> QByteArray {
            const QString src = dir.filePath(name);
            QFile f(src);
            if (!f.open(QIODevice::WriteOnly)) return {};
            f.write("identique"); f.close();
            runEncrypt({src}, "pw");
            QFile out(src + ".cpdf");
            if (!out.open(QIODevice::ReadOnly)) return {};
            return out.readAll();
        };

        const QByteArray c1 = makeAndEncrypt("a.txt");
        const QByteArray c2 = makeAndEncrypt("b.txt");
        QVERIFY(!c1.isEmpty());
        QVERIFY2(c1 != c2, "Deux chiffrements du même plaintext devraient produire des ciphertexts différents");
    }

    // ── 15. Mot de passe Unicode ─────────────────────────────────────────────

    void unicodePassword()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("uni.txt");
        const QString cpdf = src + ".cpdf";
        const QString pw   = QString::fromUtf8("пароль_日本語_🔐");

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("données"); f.close();
        QVERIFY(runEncrypt({src}, pw).encryptOk());

        QVERIFY2(runDecrypt({cpdf}, "wrong").decryptFailed(),
                 "Un mauvais mot de passe devrait échouer");
        QVERIFY2(runDecrypt({cpdf}, pw).decryptOk(),
                 "Le mot de passe Unicode correct devrait réussir");
    }

    // ── 16. Batch : plusieurs fichiers ───────────────────────────────────────

    void batch_encryptDecrypt()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());

        const QStringList names = {"f1.txt", "f2.txt", "f3.txt"};
        const QString pw = "batch_pwd";
        QStringList srcs, cpdfs;

        for (const auto& name : names) {
            const QString p = dir.filePath(name);
            srcs  << p;
            cpdfs << p + ".cpdf";
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(("contenu de " + name).toLatin1());
        }

        // Chiffrement batch (1 seule dérivation Argon2)
        const auto encResult = runEncrypt(srcs, pw);
        QCOMPARE(encResult.messages.filter(errorCodeToString(CRYPT_SUCCESS)).count(), 3);

        // Déchiffrement batch
        const auto decResult = runDecrypt(cpdfs, pw);
        QCOMPARE(decResult.messages.filter(errorCodeToString(DECRYPT_SUCCESS)).count(), 3);
    }

    // ── 17. Intégrité du contenu après roundtrip ────────────────────────────
    // Vérifie byte-à-byte sur des données binaires non-triviales.

    void contentIntegrity_binary()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString src  = dir.filePath("binary.bin");
        const QString cpdf = src + ".cpdf";

        // 300 octets avec toutes les valeurs 0x00–0xFF cycliques
        QByteArray plain(300, '\x00');
        for (int i = 0; i < 300; ++i) plain[i] = static_cast<char>(i % 256);

        QFile f(src); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(plain); f.close();
        QVERIFY(runEncrypt({src}, "pw_bin").encryptOk());
        QVERIFY(runDecrypt({cpdf}, "pw_bin").decryptOk());

        bool found = false;
        for (const auto& fi : QDir(dir.path()).entryInfoList({"*.bin"}, QDir::Files)) {
            QFile r(fi.filePath());
            if (!r.open(QIODevice::ReadOnly)) continue;
            if (r.readAll() == plain) { found = true; break; }
        }
        QVERIFY2(found, "Intégrité binaire byte-à-byte échouée");
    }
};

QTEST_MAIN(TestArsenic)
#include "test_crypto.moc"
