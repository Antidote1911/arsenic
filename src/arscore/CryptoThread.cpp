#include "CryptoThread.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include "botan_all.h"
#include "messages.h"
#include "utils.h"

#include <array>
#include <bit>
#include <concepts>
#include <cstring>
#include <future>
#include <ranges>
#include <span>

using namespace Botan;

// ── Helpers little-endian (C++20 : std::bit_cast + std::endian) ──────────────

template<std::integral T>
static void appendLE(std::vector<uint8_t>& v, T val)
{
    auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(val);
    if constexpr (std::endian::native == std::endian::big)
        std::ranges::reverse(bytes);
    v.insert(v.end(), bytes.begin(), bytes.end());
}

template<std::integral T>
static T readLE(std::span<const uint8_t> v, size_t off)
{
    std::array<uint8_t, sizeof(T)> bytes;
    std::copy_n(v.begin() + static_cast<std::ptrdiff_t>(off), sizeof(T), bytes.begin());
    if constexpr (std::endian::native == std::endian::big)
        std::ranges::reverse(bytes);
    return std::bit_cast<T>(bytes);
}

// ── Constantes ────────────────────────────────────────────────────────────────

static constexpr char    CPDF_MAGIC[4] = {'C', 'P', 'D', 'F'};
static constexpr uint8_t CPDF_VERSION  = 1;
static constexpr size_t  CHUNK_SIZE    = 1u << 20; // 1 MiB

static constexpr uint32_t ARGON_MEM  = 65536;
static constexpr uint32_t ARGON_ITER = 3;
static constexpr uint32_t ARGON_PAR  = 4;

// ── Tailles de clés et nonces par couche AEAD ─────────────────────────────────
static constexpr size_t K1 = 64; // AES-256/SIV key  (2 × 256 bits)
static constexpr size_t K2 = 32; // Serpent/GCM key
static constexpr size_t K3 = 32; // XChaCha20Poly1305 key
static constexpr size_t K4 = 32; // HMAC-SHA256 MAC key (global file authentication)
static constexpr size_t N1 = 16; // AES-256/SIV nonce
static constexpr size_t N2 = 12; // Serpent/GCM nonce
static constexpr size_t N3 = 24; // XChaCha20Poly1305 nonce

// DEK : K1+K2+K3+K4 (160 oct.) + origSize encodé (8 oct.) = 168 oct.
// origSize est chiffré dans la DEK → taille exacte invisible sans mot de passe.
static constexpr size_t DEK_KEYS_LEN = K1 + K2 + K3 + K4; // 160
static constexpr size_t DEK_LEN      = DEK_KEYS_LEN + 8;   // 168

// Overhead par chunk : nonces aléatoires(52) + 3 tags AEAD(48) = 100 oct.
// Tous les chunks font CHUNK_SIZE + OVERHEAD ; le dernier est paddé à CHUNK_SIZE.
static constexpr size_t NONCE_PER_CHUNK = N1 + N2 + N3; // 52
static constexpr size_t OVERHEAD        = NONCE_PER_CHUNK + 48; // 100

// Trailer MAC global (HMAC-SHA256 sur header + tous les chunks chiffrés)
static constexpr size_t MAC_TRAILER_LEN = 32;

// KEK wrapper (XChaCha20Poly1305)
static constexpr size_t KEK_LEN       = 32;
static constexpr size_t DEK_NONCE_LEN = 24;
static constexpr size_t ENC_DEK_LEN   = DEK_LEN + 16; // 184

// Header (257 octets) :
//   Immutable (21 oct.) : magic(4) + ver(1) + file_id(16)  ← AD de chaque chunk
//   Mutable   (236 oct.): kdf_salt(16) + argon_mem(4) + argon_iter(4) + argon_par(4)
//                         + dek_nonce(24) + enc_dek(184)
static constexpr size_t HDR_IMMUT_SZ = 21;
static constexpr size_t HDR_KDF_SZ   = 16;
static constexpr size_t HDR_ARGON_SZ = 12;
static constexpr size_t HDR_MUT_SZ   = HDR_KDF_SZ + HDR_ARGON_SZ + DEK_NONCE_LEN + ENC_DEK_LEN; // 236
static constexpr size_t HDR_SZ       = HDR_IMMUT_SZ + HDR_MUT_SZ; // 257

// Offsets dans le header
static constexpr size_t OFF_KDF_SALT   = 21;
static constexpr size_t OFF_ARGON_MEM  = 37;
static constexpr size_t OFF_ARGON_ITER = 41;
static constexpr size_t OFF_ARGON_PAR  = 45;
static constexpr size_t OFF_DEK_NONCE  = 49;
static constexpr size_t OFF_ENC_DEK    = 73;

// ── KDF ───────────────────────────────────────────────────────────────────────

static secure_vector<uint8_t> deriveKEK(const char*                 password,
                                          size_t                      password_len,
                                          const std::vector<uint8_t>& kdf_salt,
                                          uint32_t                    argon_mem,
                                          uint32_t                    argon_iter,
                                          uint32_t                    argon_par)
{
    auto fam     = PasswordHashFamily::create_or_throw("Argon2id");
    auto pwdhash = fam->from_params(argon_mem, argon_iter, argon_par);
    secure_vector<uint8_t> kek(KEK_LEN);
    pwdhash->derive_key(kek.data(), kek.size(),
                        password, password_len,
                        kdf_salt.data(), kdf_salt.size());
    return kek;
}

static secure_vector<uint8_t> wrapDEK(const secure_vector<uint8_t>& kek,
                                        const std::vector<uint8_t>&   nonce,
                                        const secure_vector<uint8_t>& dek)
{
	// Pour faire du XChaCha20, Botan se base automatiquement sur la taille du nonce.
    auto aead = AEAD_Mode::create_or_throw("ChaCha20Poly1305", Cipher_Dir::Encryption);
    aead->set_key(kek);
    aead->start(nonce);
    secure_vector<uint8_t> enc(dek.begin(), dek.end());
    aead->finish(enc);
    return enc;
}

static secure_vector<uint8_t> unwrapDEK(const secure_vector<uint8_t>& kek,
                                          const std::vector<uint8_t>&   nonce,
                                          const std::vector<uint8_t>&   enc_dek)
{
    auto aead = AEAD_Mode::create_or_throw("ChaCha20Poly1305", Cipher_Dir::Decryption);
    aead->set_key(kek);
    aead->start(nonce);
    secure_vector<uint8_t> dec(enc_dek.begin(), enc_dek.end());
    aead->finish(dec);
    return dec;
}

// ── Helpers AD ────────────────────────────────────────────────────────────────

static std::vector<uint8_t> buildChunkAD(const std::vector<uint8_t>& hdr,
                                           uint64_t chunkIdx, uint64_t total)
{
    std::vector<uint8_t> ad = hdr;
    ad.reserve(hdr.size() + 16);
    appendLE(ad, chunkIdx);
    appendLE(ad, total);
    return ad;
}

// ── Helpers AEAD ─────────────────────────────────────────────────────────────

static secure_vector<uint8_t> aeadEncrypt(const std::string&            name,
                                           const secure_vector<uint8_t>& key,
                                           const std::vector<uint8_t>&   nonce,
                                           const std::vector<uint8_t>&   ad,
                                           secure_vector<uint8_t>        data)
{
    auto aead = AEAD_Mode::create_or_throw(name, Cipher_Dir::Encryption);
    aead->set_key(key);
    aead->set_associated_data(ad);
    aead->start(nonce);
    aead->finish(data);
    return data;
}

static secure_vector<uint8_t> aeadDecrypt(const std::string&            name,
                                           const secure_vector<uint8_t>& key,
                                           const std::vector<uint8_t>&   nonce,
                                           const std::vector<uint8_t>&   ad,
                                           secure_vector<uint8_t>        data)
{
    auto aead = AEAD_Mode::create_or_throw(name, Cipher_Dir::Decryption);
    aead->set_key(key);
    aead->set_associated_data(ad);
    aead->start(nonce);
    aead->finish(data);
    return data;
}

// ── Chunk encryption : nonces aléatoires par chunk ───────────────────────────
// Sortie : N1(16) | N2(12) | N3(24) | ciphertext(CHUNK_SIZE + 48 tags)

static std::vector<uint8_t> encryptChunk(secure_vector<uint8_t>        plain,
                                           const secure_vector<uint8_t>& mat,
                                           const std::vector<uint8_t>&   hdr,
                                           uint64_t ci, uint64_t total)
{
    size_t off = 0;
    auto slice = [&](size_t n) {
        secure_vector<uint8_t> v(mat.begin()+off, mat.begin()+off+n); off += n; return v;
    };
    const auto k1 = slice(K1);
    const auto k2 = slice(K2);
    const auto k3 = slice(K3);

    AutoSeeded_RNG rng;
    std::vector<uint8_t> n1(N1), n2(N2), n3(N3);
    rng.randomize(n1.data(), N1);
    rng.randomize(n2.data(), N2);
    rng.randomize(n3.data(), N3);

    const auto ad = buildChunkAD(hdr, ci, total);

    plain = aeadEncrypt("AES-256/SIV",      k1, n1, ad, std::move(plain));
    plain = aeadEncrypt("Serpent/GCM",      k2, n2, ad, std::move(plain));
    plain = aeadEncrypt("ChaCha20Poly1305", k3, n3, ad, std::move(plain));

    std::vector<uint8_t> result;
    result.reserve(NONCE_PER_CHUNK + plain.size());
    result.insert(result.end(), n1.begin(), n1.end());
    result.insert(result.end(), n2.begin(), n2.end());
    result.insert(result.end(), n3.begin(), n3.end());
    result.insert(result.end(), plain.begin(), plain.end());
    return result;
}

// ── Chunk decryption : nonces lus depuis le préfixe du bloc ──────────────────
// Entrée : N1(16) | N2(12) | N3(24) | ciphertext

static secure_vector<uint8_t> decryptChunk(std::vector<uint8_t>          chunk,
                                             const secure_vector<uint8_t>& mat,
                                             const std::vector<uint8_t>&   hdr,
                                             uint64_t ci, uint64_t total)
{
    size_t pos = 0;
    const std::vector<uint8_t> n1(chunk.begin() + pos, chunk.begin() + pos + N1); pos += N1;
    const std::vector<uint8_t> n2(chunk.begin() + pos, chunk.begin() + pos + N2); pos += N2;
    const std::vector<uint8_t> n3(chunk.begin() + pos, chunk.begin() + pos + N3); pos += N3;
    secure_vector<uint8_t> cipher(chunk.begin() + pos, chunk.end());

    size_t off = 0;
    auto slice = [&](size_t n) {
        secure_vector<uint8_t> v(mat.begin()+off, mat.begin()+off+n); off += n; return v;
    };
    const auto k1 = slice(K1);
    const auto k2 = slice(K2);
    const auto k3 = slice(K3);

    const auto ad = buildChunkAD(hdr, ci, total);

    cipher = aeadDecrypt("ChaCha20Poly1305", k3, n3, ad, std::move(cipher));
    cipher = aeadDecrypt("Serpent/GCM",      k2, n2, ad, std::move(cipher));
    cipher = aeadDecrypt("AES-256/SIV",      k1, n1, ad, std::move(cipher));
    return cipher;
}

// ── Utilitaire d'écriture ─────────────────────────────────────────────────────

static bool writeAll(QFile& f, const void* data, qint64 n)
{
    return f.write(static_cast<const char*>(data), n) == n;
}

// ── Implémentation Crypto_Thread ──────────────────────────────────────────────

Crypto_Thread::Crypto_Thread(QObject* parent)
    : QThread(parent)
{}

void Crypto_Thread::setParam(bool direction,
                              QStringList const& filenames,
                              const QString& password,
                              const QString& /*algo*/,
                              quint32 /*argonmem*/,
                              quint32 /*argoniter*/,
                              bool deletefile)
{
    m_filenames      = filenames;
    const QByteArray utf8 = password.toUtf8();
    m_password.assign(utf8.constData(), utf8.constData() + utf8.size());
    m_direction      = direction;
    m_changePassword = false;
    m_deletefile     = deletefile;
}

void Crypto_Thread::setChangePasswordParam(QStringList const& filenames,
                                            const QString& oldPassword,
                                            const QString& newPassword)
{
    m_filenames      = filenames;
    const QByteArray oldUtf8 = oldPassword.toUtf8();
    m_password.assign(oldUtf8.constData(), oldUtf8.constData() + oldUtf8.size());
    const QByteArray newUtf8 = newPassword.toUtf8();
    m_newPassword.assign(newUtf8.constData(), newUtf8.constData() + newUtf8.size());
    m_changePassword = true;
    m_deletefile     = false;
}

void Crypto_Thread::abort()
{
    m_aborted = true;
}

void Crypto_Thread::run()
{
    if (m_changePassword) {
        for (auto& inputFileName : m_filenames) {
            if (m_aborted) break;

            QFileInfo src_info(inputFileName);
            if (!src_info.exists() || !src_info.isFile()) {
                emit statusMessage(errorCodeToString(SRC_NOT_FOUND));
                continue;
            }

            emit statusMessage("");
            emit statusMessage(
                QDateTime::currentDateTime().toString("dddd dd MMMM yyyy (hh:mm:ss)")
                + " changing password of " + inputFileName);

            const quint32 result = cpdfChangePassword(inputFileName);
            emit statusMessage(errorCodeToString(result));
        }
        m_password    = secure_vector<char>();
        m_newPassword = secure_vector<char>();
        m_changePassword = false;
        m_aborted = false;
        return;
    }

    // KEK : dérivé une seule fois pour toute la session (batch de fichiers).
    // cachedSalt permet de réutiliser le KEK au déchiffrement si le sel ne change pas.
    secure_vector<uint8_t> kek;
    std::vector<uint8_t>   kdfSalt;

    if (m_direction) {
        AutoSeeded_RNG rng;
        kdfSalt.resize(HDR_KDF_SZ);
        rng.randomize(kdfSalt.data(), kdfSalt.size());
        emit statusMessage("Argon2 passphrase derivation... Please wait.");
        kek = deriveKEK(m_password.data(), m_password.size(), kdfSalt, ARGON_MEM, ARGON_ITER, ARGON_PAR);
        m_password = secure_vector<char>(); // plus besoin après la dérivation KEK
    }

    for (auto& inputFileName : m_filenames) {
        if (m_aborted) {
            m_password = secure_vector<char>();
            return;
        }

        QFileInfo src_info(inputFileName);
        if (!src_info.exists() || !src_info.isFile()) {
            emit statusMessage("SRC_CANNOT_OPEN_READ");
            return;
        }

        emit statusMessage("");
        emit statusMessage(
            QDateTime::currentDateTime().toString("dddd dd MMMM yyyy (hh:mm:ss)")
            + (m_direction ? " encryption of " : " decryption of ")
            + inputFileName);

        const quint32 result = m_direction
            ? cpdfEncrypt(inputFileName, kek, kdfSalt)
            : cpdfDecrypt(inputFileName, kek, kdfSalt);
        emit statusMessage(errorCodeToString(result));

        if (m_aborted) {
            m_aborted = false;
            m_password = secure_vector<char>();
            return;
        }
    }
    m_password = secure_vector<char>(); // zéroïsé après toutes les dérivations (déchiffrement)
}

// ── cpdfEncrypt ───────────────────────────────────────────────────────────────

quint32 Crypto_Thread::cpdfEncrypt(const QString&                src_path,
                                    const secure_vector<uint8_t>& kek,
                                    const std::vector<uint8_t>&   kdf_salt)
{
    QFile fin(QDir::cleanPath(src_path));
    if (!fin.open(QIODevice::ReadOnly))
        return SRC_CANNOT_OPEN_READ;

    const uint64_t origSize    = static_cast<uint64_t>(QFileInfo(fin).size());
    const uint64_t totalChunks = (origSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    AutoSeeded_RNG rng;

    // file_id : lie les chunks à ce fichier ; ne change pas si le mot de passe change
    std::vector<uint8_t> file_id(16);
    rng.randomize(file_id.data(), file_id.size());

    std::vector<uint8_t> dek_nonce(DEK_NONCE_LEN);
    rng.randomize(dek_nonce.data(), dek_nonce.size());

    // DEK (168 oct.) : K1+K2+K3+K4 aléatoires + origSize chiffré
    secure_vector<uint8_t> dek(DEK_LEN);
    rng.randomize(dek.data(), DEK_KEYS_LEN);
    {
        auto bytes = std::bit_cast<std::array<uint8_t, 8>>(origSize);
        if constexpr (std::endian::native == std::endian::big)
            std::ranges::reverse(bytes);
        std::ranges::copy(bytes, dek.begin() + DEK_KEYS_LEN);
    }

    const secure_vector<uint8_t> k4_mac(dek.begin() + K1+K2+K3,
                                          dek.begin() + K1+K2+K3+K4);

    const secure_vector<uint8_t> enc_dek = wrapDEK(kek, dek_nonce, dek);

    // En-tête immutable (21 oct.) — AD de chaque chunk
    std::vector<uint8_t> immutHdr;
    immutHdr.reserve(HDR_IMMUT_SZ);
    immutHdr.insert(immutHdr.end(), CPDF_MAGIC, CPDF_MAGIC + 4);
    immutHdr.push_back(CPDF_VERSION);
    immutHdr.insert(immutHdr.end(), file_id.begin(), file_id.end());

    // En-tête complet (257 oct.) = immutable + mutable
    std::vector<uint8_t> fullHdr = immutHdr;
    fullHdr.reserve(HDR_SZ);
    fullHdr.insert(fullHdr.end(), kdf_salt.begin(), kdf_salt.end());
    appendLE(fullHdr, ARGON_MEM);
    appendLE(fullHdr, ARGON_ITER);
    appendLE(fullHdr, ARGON_PAR);
    fullHdr.insert(fullHdr.end(), dek_nonce.begin(), dek_nonce.end());
    fullHdr.insert(fullHdr.end(), enc_dek.begin(), enc_dek.end());

    const QString outPath = Utils::uniqueFileName(src_path + ".cpdf");
    QFile fout(outPath);
    if (!fout.open(QIODevice::WriteOnly))
        return DES_CANNOT_OPEN_WRITE;

    struct Guard {
        QFile& f; const QString& p; bool ok{false};
        ~Guard() { f.close(); if (!ok) QFile::remove(p); }
    } guard{fout, outPath};

    if (!writeAll(fout, fullHdr.data(), static_cast<qint64>(fullHdr.size())))
        return DES_CANNOT_OPEN_WRITE;

    // MAC global : HMAC-SHA256(K4, header || chunks chiffrés)
    auto mac = MessageAuthenticationCode::create_or_throw("HMAC(SHA-256)");
    mac->set_key(k4_mac);
    mac->update(fullHdr.data(), fullHdr.size());

    const int nThreads = std::max(1, QThread::idealThreadCount());

    for (uint64_t done = 0; done < totalChunks; ) {
        if (m_aborted) {
            emit updateProgress(src_path, 0);
            return ABORTED_BY_USER;
        }

        const uint64_t batchEnd  = std::min<uint64_t>(done + nThreads, totalChunks);
        const int      batchSize = static_cast<int>(batchEnd - done);

        std::vector<secure_vector<uint8_t>> plains(batchSize);
        for (int i = 0; i < batchSize; ++i) {
            const uint64_t ci     = done + static_cast<uint64_t>(i);
            const uint64_t realSz = std::min<uint64_t>(CHUNK_SIZE, origSize - ci * CHUNK_SIZE);
            plains[i].resize(CHUNK_SIZE);
            if (fin.read(reinterpret_cast<char*>(plains[i].data()),
                         static_cast<qint64>(realSz)) != static_cast<qint64>(realSz))
                return SRC_CANNOT_OPEN_READ;
            if (realSz < CHUNK_SIZE) // padding aléatoire du dernier chunk
                rng.randomize(plains[i].data() + realSz, CHUNK_SIZE - realSz);
        }

        std::vector<std::future<std::vector<uint8_t>>> futures;
        futures.reserve(batchSize);
        for (int i = 0; i < batchSize; ++i) {
            const uint64_t ci = done + i;
            futures.push_back(std::async(std::launch::async,
                [plain = std::move(plains[i]), &dek, &immutHdr, ci, totalChunks]() {
                    return encryptChunk(std::move(plain), dek, immutHdr, ci, totalChunks);
                }));
        }

        for (int i = 0; i < batchSize; ++i) {
            const auto enc = futures[i].get();
            mac->update(enc.data(), enc.size());
            if (!writeAll(fout, enc.data(), static_cast<qint64>(enc.size())))
                return DES_CANNOT_OPEN_WRITE;
        }

        done += batchSize;
        emit updateProgress(src_path, static_cast<quint32>(done * 100 / totalChunks));
    }

    const auto macTag = mac->final();
    if (!writeAll(fout, macTag.data(), static_cast<qint64>(macTag.size())))
        return DES_CANNOT_OPEN_WRITE;

    guard.ok = true;
    emit updateProgress(src_path, 100);

    if (m_deletefile) {
        fin.close();
        QFile::remove(fin.fileName());
        emit deletedAfterSuccess(src_path);
    }
    emit addEncrypted(outPath);
    return CRYPT_SUCCESS;
}

// ── cpdfDecrypt ───────────────────────────────────────────────────────────────

quint32 Crypto_Thread::cpdfDecrypt(const QString&          src_path,
                                    secure_vector<uint8_t>& kek,
                                    std::vector<uint8_t>&   cachedSalt)
{
    QFile fin(QDir::cleanPath(src_path));
    if (!fin.open(QIODevice::ReadOnly))
        return SRC_CANNOT_OPEN_READ;

    uint8_t preamble[5];
    if (fin.read(reinterpret_cast<char*>(preamble), 5) != 5)
        return SRC_HEADER_READ_ERROR;
    if (std::memcmp(preamble, CPDF_MAGIC, 4) != 0)
        return NOT_AN_ARSENIC_FILE;
    if (preamble[4] != CPDF_VERSION)
        return NOT_AN_ARSENIC_FILE;

    std::vector<uint8_t> hdrRest(HDR_SZ - 5);
    if (fin.read(reinterpret_cast<char*>(hdrRest.data()),
                 static_cast<qint64>(hdrRest.size())) != static_cast<qint64>(hdrRest.size()))
        return SRC_HEADER_READ_ERROR;

    std::vector<uint8_t> fullHdr(preamble, preamble + 5);
    fullHdr.insert(fullHdr.end(), hdrRest.begin(), hdrRest.end());

    // Partie immutable → AD de chaque chunk
    const std::vector<uint8_t> immutHdr(fullHdr.begin(),
                                         fullHdr.begin() + HDR_IMMUT_SZ);

    const std::vector<uint8_t> kdf_salt(fullHdr.begin() + OFF_KDF_SALT,
                                          fullHdr.begin() + OFF_KDF_SALT + HDR_KDF_SZ);

    // Lire les paramètres Argon2 stockés dans le fichier
    const auto argon_mem  = readLE<uint32_t>(fullHdr, OFF_ARGON_MEM);
    const auto argon_iter = readLE<uint32_t>(fullHdr, OFF_ARGON_ITER);
    const auto argon_par  = readLE<uint32_t>(fullHdr, OFF_ARGON_PAR);

    // Validation : bornes serrées pour éviter un DoS par fichier malveillant.
    // Max mem = 512 MiB (8× la valeur produite par Arsenic), iter/par = 16.
    if (argon_mem < 8    || argon_mem > 524288 ||
        argon_iter < 1   || argon_iter > 16    ||
        argon_par  < 1   || argon_par  > 16)
        return NOT_AN_ARSENIC_FILE;

    const std::vector<uint8_t> dek_nonce(fullHdr.begin() + OFF_DEK_NONCE,
                                           fullHdr.begin() + OFF_DEK_NONCE + DEK_NONCE_LEN);
    const std::vector<uint8_t> enc_dek(fullHdr.begin() + OFF_ENC_DEK,
                                         fullHdr.begin() + OFF_ENC_DEK + ENC_DEK_LEN);

    // Utiliser le cache KEK de session ou dériver si le sel diffère
    if (kek.empty() || cachedSalt != kdf_salt) {
        emit statusMessage("Argon2 passphrase derivation... Please wait.");
        kek        = deriveKEK(m_password.data(), m_password.size(), kdf_salt, argon_mem, argon_iter, argon_par);
        cachedSalt = kdf_salt;
    }

    secure_vector<uint8_t> dek;
    try {
        dek = unwrapDEK(kek, dek_nonce, enc_dek);
    } catch (const std::exception&) {
        return DECRYPT_FAIL;
    }

    // Extraire origSize depuis la DEK (offset DEK_KEYS_LEN = 160)
    const uint64_t origSize = readLE<uint64_t>(
        std::span<const uint8_t>(dek.data(), dek.size()), DEK_KEYS_LEN);
    const uint64_t totalChunks = (origSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    const secure_vector<uint8_t> k4_mac(dek.begin() + K1+K2+K3,
                                          dek.begin() + K1+K2+K3+K4);
    auto mac = MessageAuthenticationCode::create_or_throw("HMAC(SHA-256)");
    mac->set_key(k4_mac);
    mac->update(fullHdr.data(), fullHdr.size());

    const QFileInfo srcInfo(src_path);
    QString outBase = srcInfo.absolutePath() + QDir::separator() + srcInfo.fileName();
    if (outBase.endsWith(".cpdf", Qt::CaseInsensitive))
        outBase.chop(5);
    else if (outBase.endsWith(".arsn", Qt::CaseInsensitive))
        outBase.chop(5);
    const QString outPath = Utils::uniqueFileName(outBase);

    QFile fout(outPath);
    if (!fout.open(QIODevice::WriteOnly))
        return DES_CANNOT_OPEN_WRITE;

    struct Guard {
        QFile& f; const QString& p; bool ok{false};
        ~Guard() { f.close(); if (!ok) QFile::remove(p); }
    } guard{fout, outPath};

    const int nThreads = std::max(1, QThread::idealThreadCount());

    for (uint64_t done = 0; done < totalChunks; ) {
        if (m_aborted) {
            emit updateProgress(src_path, 0);
            return ABORTED_BY_USER;
        }

        const uint64_t batchEnd  = std::min<uint64_t>(done + nThreads, totalChunks);
        const int      batchSize = static_cast<int>(batchEnd - done);

        // Tous les chunks font CHUNK_SIZE + OVERHEAD (dernier paddé à CHUNK_SIZE)
        std::vector<std::vector<uint8_t>> ciphers(batchSize);
        for (int i = 0; i < batchSize; ++i) {
            ciphers[i].resize(CHUNK_SIZE + OVERHEAD);
            if (fin.read(reinterpret_cast<char*>(ciphers[i].data()),
                         static_cast<qint64>(CHUNK_SIZE + OVERHEAD)) != static_cast<qint64>(CHUNK_SIZE + OVERHEAD))
                return SRC_CHUNK_READ_ERROR;
            mac->update(ciphers[i].data(), ciphers[i].size());
        }

        std::vector<std::future<secure_vector<uint8_t>>> futures;
        futures.reserve(batchSize);
        for (int i = 0; i < batchSize; ++i) {
            const uint64_t ci = done + static_cast<uint64_t>(i);
            futures.push_back(std::async(std::launch::async,
                [cipher = std::move(ciphers[i]), &dek, &immutHdr, ci, totalChunks]() {
                    return decryptChunk(std::move(cipher), dek, immutHdr, ci, totalChunks);
                }));
        }

        for (int i = 0; i < batchSize; ++i) {
            secure_vector<uint8_t> plain;
            try {
                plain = futures[i].get();
            } catch (const std::exception&) {
                return DECRYPT_FAIL;
            }
            const uint64_t ci_abs    = done + static_cast<uint64_t>(i);
            const uint64_t writeSize = (ci_abs == totalChunks - 1)
                ? (origSize - ci_abs * CHUNK_SIZE)
                : static_cast<uint64_t>(plain.size());
            if (!writeAll(fout, plain.data(), static_cast<qint64>(writeSize)))
                return DES_CANNOT_OPEN_WRITE;
        }

        done += batchSize;
        emit updateProgress(src_path, static_cast<quint32>(done * 100 / totalChunks));
    }

    // Vérifier le trailer MAC — détecte troncature, ajout d'octets, toute altération globale
    std::vector<uint8_t> storedMac(MAC_TRAILER_LEN);
    if (fin.read(reinterpret_cast<char*>(storedMac.data()),
                 static_cast<qint64>(MAC_TRAILER_LEN)) != static_cast<qint64>(MAC_TRAILER_LEN))
        return GLOBAL_MAC_FAIL;
    const auto computedMac = mac->final();
    if (!constant_time_compare(computedMac.data(), storedMac.data(), MAC_TRAILER_LEN))
        return GLOBAL_MAC_FAIL;

    guard.ok = true;
    emit updateProgress(src_path, 100);

    if (m_deletefile) {
        fin.close();
        QFile::remove(fin.fileName());
        emit deletedAfterSuccess(src_path);
    }
    return DECRYPT_SUCCESS;
}

// ── cpdfChangePassword ────────────────────────────────────────────────────────
// Rewrite only the 257-byte header and 32-byte MAC trailer in place.
// The encrypted chunks are never decrypted — only streamed for HMAC recomputation.

quint32 Crypto_Thread::cpdfChangePassword(const QString& src_path)
{
    QFile f(QDir::cleanPath(src_path));
    if (!f.open(QIODevice::ReadWrite))
        return SRC_CANNOT_OPEN_READ;

    const qint64 fileSize = f.size();
    if (fileSize < static_cast<qint64>(HDR_SZ + MAC_TRAILER_LEN))
        return NOT_AN_ARSENIC_FILE;

    // Read and validate header
    std::vector<uint8_t> fullHdr(HDR_SZ);
    if (f.read(reinterpret_cast<char*>(fullHdr.data()),
               static_cast<qint64>(HDR_SZ)) != static_cast<qint64>(HDR_SZ))
        return SRC_HEADER_READ_ERROR;

    if (std::memcmp(fullHdr.data(), CPDF_MAGIC, 4) != 0)
        return NOT_AN_ARSENIC_FILE;
    if (fullHdr[4] != CPDF_VERSION)
        return NOT_AN_ARSENIC_FILE;

    const auto argon_mem  = readLE<uint32_t>(fullHdr, OFF_ARGON_MEM);
    const auto argon_iter = readLE<uint32_t>(fullHdr, OFF_ARGON_ITER);
    const auto argon_par  = readLE<uint32_t>(fullHdr, OFF_ARGON_PAR);

    if (argon_mem < 8 || argon_mem > 524288 ||
        argon_iter < 1 || argon_iter > 16    ||
        argon_par  < 1 || argon_par  > 16)
        return NOT_AN_ARSENIC_FILE;

    const std::vector<uint8_t> kdf_salt(fullHdr.begin() + OFF_KDF_SALT,
                                          fullHdr.begin() + OFF_KDF_SALT + HDR_KDF_SZ);
    const std::vector<uint8_t> dek_nonce(fullHdr.begin() + OFF_DEK_NONCE,
                                           fullHdr.begin() + OFF_DEK_NONCE + DEK_NONCE_LEN);
    const std::vector<uint8_t> enc_dek(fullHdr.begin() + OFF_ENC_DEK,
                                         fullHdr.begin() + OFF_ENC_DEK + ENC_DEK_LEN);

    // Derive old KEK and unwrap DEK — proves the old password is correct
    emit statusMessage("Argon2 old password derivation... Please wait.");
    const auto oldKek = deriveKEK(m_password.data(), m_password.size(),
                                   kdf_salt, argon_mem, argon_iter, argon_par);
    secure_vector<uint8_t> dek;
    try {
        dek = unwrapDEK(oldKek, dek_nonce, enc_dek);
    } catch (const std::exception&) {
        return WRONG_OLD_PASSWORD;
    }

    const secure_vector<uint8_t> k4_mac(dek.begin() + K1+K2+K3,
                                          dek.begin() + K1+K2+K3+K4);

    // Generate new salt + nonce, derive new KEK, re-wrap the same DEK
    AutoSeeded_RNG rng;
    std::vector<uint8_t> new_salt(HDR_KDF_SZ);
    std::vector<uint8_t> new_nonce(DEK_NONCE_LEN);
    rng.randomize(new_salt.data(), new_salt.size());
    rng.randomize(new_nonce.data(), new_nonce.size());

    emit statusMessage("Argon2 new password derivation... Please wait.");
    const auto newKek = deriveKEK(m_newPassword.data(), m_newPassword.size(),
                                   new_salt, ARGON_MEM, ARGON_ITER, ARGON_PAR);
    const auto new_enc_dek = wrapDEK(newKek, new_nonce, dek);

    // Build new header: keep immutable part (file_id), replace mutable part
    std::vector<uint8_t> newHdr(fullHdr.begin(), fullHdr.begin() + HDR_IMMUT_SZ);
    newHdr.reserve(HDR_SZ);
    newHdr.insert(newHdr.end(), new_salt.begin(), new_salt.end());
    appendLE(newHdr, ARGON_MEM);
    appendLE(newHdr, ARGON_ITER);
    appendLE(newHdr, ARGON_PAR);
    newHdr.insert(newHdr.end(), new_nonce.begin(), new_nonce.end());
    newHdr.insert(newHdr.end(), new_enc_dek.begin(), new_enc_dek.end());

    // Read stored MAC from end of file
    std::vector<uint8_t> storedMac(MAC_TRAILER_LEN);
    if (!f.seek(fileSize - static_cast<qint64>(MAC_TRAILER_LEN)))
        return SRC_HEADER_READ_ERROR;
    if (f.read(reinterpret_cast<char*>(storedMac.data()),
               static_cast<qint64>(MAC_TRAILER_LEN)) != static_cast<qint64>(MAC_TRAILER_LEN))
        return SRC_HEADER_READ_ERROR;

    // Single streaming pass: compute old MAC (verify) and new MAC simultaneously
    auto oldMac = MessageAuthenticationCode::create_or_throw("HMAC(SHA-256)");
    auto newMac = MessageAuthenticationCode::create_or_throw("HMAC(SHA-256)");
    oldMac->set_key(k4_mac);
    newMac->set_key(k4_mac);
    oldMac->update(fullHdr.data(), fullHdr.size());
    newMac->update(newHdr.data(), newHdr.size());

    if (!f.seek(static_cast<qint64>(HDR_SZ)))
        return SRC_HEADER_READ_ERROR;

    const qint64 chunksAreaLen = fileSize - static_cast<qint64>(HDR_SZ + MAC_TRAILER_LEN);
    constexpr qint64 BUF_SZ = static_cast<qint64>(CHUNK_SIZE);
    std::vector<uint8_t> buf(static_cast<size_t>(BUF_SZ));
    qint64 remaining = chunksAreaLen;
    qint64 processed = 0;

    while (remaining > 0) {
        if (m_aborted)
            return ABORTED_BY_USER;
        const qint64 toRead = std::min(remaining, BUF_SZ);
        if (f.read(reinterpret_cast<char*>(buf.data()), toRead) != toRead)
            return SRC_CHUNK_READ_ERROR;
        oldMac->update(buf.data(), static_cast<size_t>(toRead));
        newMac->update(buf.data(), static_cast<size_t>(toRead));
        processed += toRead;
        remaining -= toRead;
        emit updateProgress(src_path,
            static_cast<quint32>(processed * 100 / (chunksAreaLen > 0 ? chunksAreaLen : 1)));
    }

    // Verify old MAC before committing any change
    const auto computedOldMac = oldMac->final();
    if (!constant_time_compare(computedOldMac.data(), storedMac.data(), MAC_TRAILER_LEN))
        return GLOBAL_MAC_FAIL;

    const auto newMacTag = newMac->final();

    // Write new header at offset 0 (in-place, no chunk touched)
    if (!f.seek(0))
        return DES_CANNOT_OPEN_WRITE;
    if (f.write(reinterpret_cast<const char*>(newHdr.data()),
                static_cast<qint64>(newHdr.size())) != static_cast<qint64>(newHdr.size()))
        return DES_CANNOT_OPEN_WRITE;

    // Write new MAC at end
    if (!f.seek(fileSize - static_cast<qint64>(MAC_TRAILER_LEN)))
        return DES_CANNOT_OPEN_WRITE;
    if (f.write(reinterpret_cast<const char*>(newMacTag.data()),
                static_cast<qint64>(newMacTag.size())) != static_cast<qint64>(newMacTag.size()))
        return DES_CANNOT_OPEN_WRITE;

    emit updateProgress(src_path, 100);
    return CHANGE_PASSWORD_SUCCESS;
}
