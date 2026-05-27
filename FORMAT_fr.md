# Format de fichier chiffré CPDF

Ce document décrit le format binaire exact des fichiers produits par Arsenic
lors du chiffrement de fichiers arbitraires. L'extension conventionnelle est `.cpdf`.

---

## Version du format

| Version | Octets en-tête | Description                                                    |
|---------|----------------|----------------------------------------------------------------|
| `0x01`  | 257            | KEK/DEK, nonces aléatoires par chunk, origSize chiffré, padding CHUNK_SIZE, MAC global HMAC-SHA256 |

---

## Vue d'ensemble

```
┌─────────────────────────────────────────────────────────────┐
│  EN-TÊTE  (257 octets, fixe)                                │
├─────────────────────────────────────────────────────────────┤
│  CHUNK 0  (CHUNK_SIZE + 100 octets)                         │
├─────────────────────────────────────────────────────────────┤
│  CHUNK 1  (CHUNK_SIZE + 100 octets)                         │
├─────────────────────────────────────────────────────────────┤
│  …                                                          │
├─────────────────────────────────────────────────────────────┤
│  CHUNK N-1  (CHUNK_SIZE + 100 octets)                       │
├─────────────────────────────────────────────────────────────┤
│  TRAILER MAC  (32 octets)                                   │
└─────────────────────────────────────────────────────────────┘
```

**Chaque** chunk chiffré fait exactement `CHUNK_SIZE + 100` octets. Le dernier
chunk est rembourré avec des octets aléatoires jusqu'à `CHUNK_SIZE` avant
chiffrement. `origSize` est stocké chiffré dans la DEK pour que le destinataire
puisse supprimer le padding ; il n'apparaît jamais dans le header en clair.

La taille sur disque révèle seulement `⌈origSize / 1 MiB⌉` (le nombre de chunks),
pas la taille exacte du fichier d'origine.

Taille totale du fichier chiffré :

```
257 + ⌈taille_originale / 1 048 576⌉ × 1 048 676 + 32  octets
```

(où `1 048 676 = CHUNK_SIZE + OVERHEAD = 1 048 576 + 100`)

---

## En-tête (257 octets)

### Partie immutable (21 octets — utilisée dans les AD de chunk)

| Offset | Longueur | Contenu                                                          |
|--------|----------|------------------------------------------------------------------|
| 0      | 4        | Nombre magique ASCII : `43 50 44 46` (`CPDF`)                   |
| 4      | 1        | Version du format : `0x01`                                      |
| 5      | 16       | **File ID** (aléatoire, CSPRNG, ne change jamais)               |

`origSize` n'est **pas** stocké ici. Il se trouve dans la DEK chiffrée et ne peut
pas être lu sans le mot de passe.

### Partie mutable (236 octets — réécrite lors d'un changement de mot de passe)

| Offset | Longueur | Contenu                                                       |
|--------|----------|---------------------------------------------------------------|
| 21     | 16       | Sel KDF (aléatoire, CSPRNG)                                  |
| 37     | 4        | `argon_mem`  : coût mémoire Argon2id (uint32 LE, KiB)        |
| 41     | 4        | `argon_iter` : nombre d'itérations  (uint32 LE)              |
| 45     | 4        | `argon_par`  : parallélisme          (uint32 LE)             |
| 49     | 24       | Nonce de wrapping DEK (aléatoire, CSPRNG)                    |
| 73     | 184      | DEK chiffrée = XChaCha20Poly1305(KEK, nonce, DEK) + tag 16 oct. |

---

## Séparation KEK / DEK

### Principe

```
Mot de passe + sel KDF  →  Argon2id  →  KEK (32 octets)
                                              │
                                              ▼
DEK aléatoire (168 oct.)  →  XChaCha20Poly1305(KEK)  →  DEK chiffrée (en-tête)
                │
                ├──→ K1, K2, K3 : clés AEAD par couche
                ├──→ K4 : clé HMAC-SHA256  (MAC global)
                └──→ origSize (8 oct.)  ←── chiffré, invisible sans mot de passe
```

### Avantages

- **Changement de mot de passe** : re-dériver le KEK et rechiffrer uniquement les
  184 octets de DEK dans l'en-tête. Le reste du fichier (même 50 Go) n'est pas touché.

- **Performance batch** : Argon2id tourne **une seule fois** pour toute la session.

- **Confidentialité de la taille** : `origSize` exact est invisible sans le mot de
  passe. La taille sur disque révèle seulement la plage de 1 MiB
  `[(N−1)×1MiB+1, N×1MiB]`.

### Paramètres Argon2id (lus depuis le fichier, jamais codés en dur)

| Paramètre    | Valeur (par défaut) | Source lors du déchiffrement   |
|--------------|---------------------|--------------------------------|
| Mémoire      | 65 536 KiB (64 MiB) | champ `argon_mem` (offset 37)  |
| Itérations   | 3                   | champ `argon_iter` (offset 41) |
| Parallélisme | 4                   | champ `argon_par` (offset 45)  |
| Entrée       | mot de passe UTF-8  | —                              |
| Sel          | 16 octets (offset 21) | —                            |
| Sortie       | **32 octets** (KEK) | —                              |

### Répartition de la DEK (168 octets)

```
Offset  Longueur  Usage
──────  ────────  ─────────────────────────────────────────────────────────
 0       64        K1 : clé AES-256/SIV  (2 × 256 bits)
64       32        K2 : clé Serpent-256/GCM
96       32        K3 : clé XChaCha20Poly1305
128      32        K4 : clé HMAC-SHA256  (authentification globale du fichier)
160       8        origSize : taille originale (uint64 LE) — chiffrée ici
──────────────────────────────────────────────────────────────────────────
Total : 168 octets
```

Les nonces ne sont **pas** stockés dans la DEK. Chaque chunk génère trois nonces
indépendants aléatoires (16 + 12 + 24 = 52 octets) stockés en préfixe en clair.

---

## Découpage en chunks et padding

- Nombre de chunks : `N = ⌈taille_originale / 1 048 576⌉` (0 si taille == 0)
- Chaque chunk — y compris le dernier — est rembourré à exactement **1 048 576 octets**
  avant chiffrement (octets aléatoires ajoutés après les vraies données).
- Chaque chunk chiffré : `52 (nonces) + 1 048 576 (clair paddé) + 48 (tags) = 1 048 676 octets`
- Au déchiffrement, `origSize` extrait de la DEK indique combien d'octets du dernier
  chunk sont de vraies données.

---

## Nonce par chunk

Chaque chunk stocke trois nonces indépendants aléatoires en préfixe en clair :

```
┌──────────────────────────────────────────────────┐
│  N1 : nonce AES-256/SIV        (16 octets)       │
│  N2 : nonce Serpent-256/GCM    (12 octets)       │
│  N3 : nonce XChaCha20Poly1305  (24 octets)       │
└──────────────────────────────────────────────────┘
     Total : 52 octets par chunk
```

---

## Données associées (AD) par chunk

Chaque couche AEAD reçoit les mêmes **37 octets** de données associées :

```
┌──────────────────────────────────────────────────────┐
│  Partie IMMUTABLE de l'en-tête  (21 octets)          │
│  Index du chunk  (uint64, little-endian, 8 octets)   │
│  Nombre total de chunks  (uint64, LE, 8 octets)      │
└──────────────────────────────────────────────────────┘
```

---

## Chiffrement d'un chunk (ordre d'application)

```
Clair paddé (1 048 576 octets)
  │
  ▼  1. AES-256/SIV      clé K1 (64 oct.)  nonce N1 (aléatoire)  AD 37 oct.  tag +16
  │
  ▼  2. Serpent-256/GCM  clé K2 (32 oct.)  nonce N2 (aléatoire)  AD 37 oct.  tag +16
  │
  ▼  3. XChaCha20Poly1305 clé K3 (32 oct.)  nonce N3 (aléatoire)  AD 37 oct.  tag +16
  │
Chunk de sortie : N1(16) | N2(12) | N3(24) | chiffré(1 048 576 + 48 tags)
```

---

## Déchiffrement d'un chunk (ordre inverse)

```
Chunk d'entrée (1 048 676 oct.) : N1(16) | N2(12) | N3(24) | chiffré(1 048 624)
  →  XChaCha20Poly1305  →  Serpent/GCM  →  AES-256/SIV  →  1 048 576 octets
```

Dernier chunk : écrire seulement `origSize mod 1 048 576` octets (supprimer le padding).
Si `origSize` est un multiple de 1 MiB : écrire 1 048 576 octets (pas de padding).

Si l'un des tags est invalide, le déchiffrement s'arrête et le fichier partiel
est supprimé.

---

## Trailer MAC global

```
HMAC-SHA256( K4,  header(257 oct.) || chunk0 || chunk1 || … || chunkN-1 )
```

- Calculé pendant le chiffrement par accumulation streaming.
- Vérifié pendant le déchiffrement après tous les chunks, avant `DECRYPT_SUCCESS`.
  En cas d'échec, le fichier de sortie est supprimé (code `GLOBAL_MAC_FAIL`).

---

## Parallélisme

Chunks traités par **batches** de `N` threads (`QThread::idealThreadCount()`).
Lecture séquentielle → chiffrement/déchiffrement parallèle → écriture séquentielle.
Le MAC global est mis à jour séquentiellement (dans l'ordre du fichier).

### Pourquoi `std::async` / `std::future` et non `QThreadPool` / `QRunnable`

Le parallélisme au niveau des chunks est assuré par `std::async(std::launch::async, …)`
à l'intérieur d'un unique `QThread` propriétaire. Les quatre raisons qui excluent
`QThreadPool` / `QRunnable` :

1. **Contrainte d'ordre du MAC global.**
   Le HMAC-SHA256 de fin de fichier doit accumuler les données dans l'ordre strict du
   fichier (en-tête, puis chaque chunk chiffré dans l'ordre croissant des index).
   Avec des runnables soumis à un pool, imposer cet ordre séquentiel exigerait une
   barrière ou une file de résultats supplémentaire. Avec des `std::future` collectés
   dans l'ordre d'index, c'est trivial : `futures[i].get()` garantit la séquence.

2. **Durée de vie du mot de passe sécurisé.**
   `m_password` (`secure_vector<char>`) doit être zéroïsé dès que la dérivation KEK
   est terminée. Un thread unique et propriétaire rend ce point de zéroïsation
   non ambigu. Partager le mot de passe entre des runnables obligerait soit à le copier
   dans chaque tâche (multipliant les zones en clair sur le heap), soit à le protéger
   par un mutex — ce qui augmente la surface mémoire exposée.

3. **Cache KEK de session.**
   Pour un batch de déchiffrement, le KEK n'est re-dérivé que si le sel KDF change
   d'un fichier à l'autre. Cet état (`kek` + `cachedSalt`) vit naturellement dans un
   contexte d'exécution séquentiel unique. Le coordonner entre des tâches de pool
   nécessiterait une synchronisation explicite et compliquerait le chemin d'annulation
   (`m_aborted`).

4. **Concurrence bornée déjà atteinte.**
   La taille du batch est exactement `QThread::idealThreadCount()`. `std::async` crée
   précisément le bon nombre de threads OS par batch, saturant le CPU de la même façon
   qu'un pool, sans l'overhead de la file d'attente ni l'indirection de l'interface
   `QRunnable`.

---

## Exemple de taille de fichier

| Taille originale | Chunks | Taille chiffrée                                         |
|-----------------|--------|---------------------------------------------------------|
| 0 B             | 0      | 257 + 32 = **289 B**                                   |
| 1 B             | 1      | 257 + 1 048 676 + 32 = **1 048 965 B** (~1 MiB)       |
| 1 048 576 B (1 MiB) | 1  | 257 + 1 048 676 + 32 = **1 048 965 B**                 |
| 1 048 577 B     | 2      | 257 + 2 × 1 048 676 + 32 = **2 097 641 B**            |
| 10 485 760 B (10 MiB) | 10 | 257 + 10 × 1 048 676 + 32 = **10 487 049 B**       |
