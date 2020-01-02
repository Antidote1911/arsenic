[![Build Status](https://travis-ci.org/Antidote1911/Arsenic.svg?branch=master)](https://travis-ci.org/Antidote1911/Arsenic)
[![Build status](https://ci.appveyor.com/api/projects/status/eid3dqq4c28u7sf4?svg=true)](https://ci.appveyor.com/project/Antidote1911/arsenic)

# Arsenic
**Un outil simple pour chiffrer vos fichiers et dossiers.**


<img src='screenshots/main.png'/>

## Simple Description: ##
Arsenic et un logiciel simple et legé destiné à chiffrer localement des listes de fichiers ou meme des dossiers entiers. L'utilisateur peu choisir entre trois algorithmes:

- [XChaCha20Poly1305](https://botan.randombit.net/handbook/api_ref/cipher_modes.html#chacha20poly1305)
- [AES256](https://en.wikipedia.org/wiki/Advanced_Encryption_Standard)/[GCM](https://en.wikipedia.org/wiki/Galois/Counter_Mode)
- [Serpent](https://en.wikipedia.org/wiki/Serpent_(cipher))/[GCM](https://en.wikipedia.org/wiki/Galois/Counter_Mode)

Quelques outils sont inclus : calculateur de hash, generateur de mot de passe etc...

A l'origine, Arsenic est un fork de [QtCrypt](https://github.com/trashctor/QtCrypt) publié en 2015 par [trashctor](https://github.com/trashctor).<br>
https://github.com/trashctor/QtCrypt<br>

Voici les ajouts ou modification apportés par Arsenic:
- Passage de Qt4 to Qt5
- Remplacemment d'une vieille version de [libsodium](https://github.com/jedisct1/libsodium "Strong cryptographic library") à une version recente de [Botan](https://botan.randombit.net/ "Strong cryptographic library").
- La dérivation de la clé à partir du mot de passe est maintenant confiée à [Argon2](https://en.wikipedia.org/wiki/Argon2)
- L'utilisateur peu maintenant choisir entre trois algorithmes de chiffrement [AEAD](https://en.wikipedia.org/wiki/Authenticated_encryption).
([XChaCha20Poly1305](https://botan.randombit.net/handbook/api_ref/cipher_modes.html#chacha20poly1305), [AES256](https://en.wikipedia.org/wiki/Advanced_Encryption_Standard)/[GCM](https://en.wikipedia.org/wiki/Galois/Counter_Mode), or [Serpent](https://en.wikipedia.org/wiki/Serpent_(cipher))/[GCM](https://en.wikipedia.org/wiki/Galois/Counter_Mode))
- Le logiciel peu désormais être utilisé en ligne de commande
- ajout d'un paneau de configuration pour les utilisateurs avancés
- ajout d'un generateur de mot de passe
- Ajout d'un calculateur de hash pour les algoritmes suivants :
<em>(SHA-3, SHA-1, SHA-224, SHA-256, SHA-384, SHA-512, SHA-512-256, Skein-512, Keccak-1600, Whirlpool, Blake2b, SHAKE-128, SHAKE-256, GOST-34.11, SM3, Tiger, Streebog-256, Streebog-512, RIPEMD-160, Adler32, MD4, MD5, CRC24, CRC32)</em>
- Ajout d'un skin sombre

Un grand merci à :
- [trashctor](https://github.com/trashctor) pour QtCrypt.
- [Jack Lloyd de randombit.net](https://botan.randombit.net) pour la superbe bibliotheque cryptographique c++, Botan. Vous pouvez trouver son Github [ici](https://github.com/randombit/botan).<br>
Botan est distribuée sous license simplifée [BSD license](https://botan.randombit.net/license.txt)

## Mode ligne de commande: ##
**ATTENTION, le mode ligne de commande ne supporte pas les espaces dans les parametres. (nom de fichier, mot de passe)**
L'exemple suivant chiffre le fichier "test.mkv" avec le mot de passe "mysuperbadpassphrase" et l'e-mail "myemail@cool.com". Le resultat est un fichier nommé "test.mkv.arsenic" mais il peu être renommé car le nom original à été écrit dans le fichier chiffré.

```bash
    ./arsenic -e -p mysuperbadpassphrase -n myemail@cool.com test.mkv
    # Pour déchiffrer ce fichier :
    ./arsenic -d -p mysuperbadpassphrase test.mkv.arsenic
    # Voir l'aide :
    ./arsenic -h
    # Voir la version d'arsenic :
    ./arsenic -v
```

## Description technique: ##
- Premierement, le fichier ou dossier est compressé au format zip
- Une clé maitre de 96 bytes est générée par Argon2 à partir du mot de passe et d'un "sel" aléatoire de 16 bytes (128 bits)
- Cette clée est splitée en trois clées de 32 bytes chacunes. clée1, clée2, clée3
- Un vecteur d'initialisation (ou nonce) de 128 bits est généré.
- Ecriture en tete de fichier d'un "magic number"
- Ecriture de la version d'Arsenic
- Ecriture des parametres d'Argon2 parameters (memory limit, iterations, sel)
- Ecriture du nom de l'algorithme utilisé
- Ecriture de l'email
- Ecriture du vecteur d'initialisation (VI)
- chiffrage (avec clé1 et VI) de la taille de l'entete du fichier, et écriture
- chiffrage (avec clé2 et VI +1) du nom du fichier et de sa taille
- chiffrage (avec clé3 and VI +2) du fichier original.
- Ecriture du tag d'authentication de 16 bytes.

Notes:<br>
Le vecteur d'initialisation est incrémenté avant chaque utilisation pour etre certain de ne jamais être réutilisé avec une même clé.<br>
L'ecriture de l'email ou du pseudo, se fait en clair et n'est pas secret. Ils sont uniquemment utilisé par l'algoritme d'authentification. Vous pouvez lire plus d'information à ce sujet sur [wikipedia](https://en.wikipedia.org/wiki/Authenticated_encryption)<br>
Le processus est optimisé pour les gros fichiers. Arsenic lit et ecrit les fichiers morceaux par morceaux.

## Developeurs: ##
The application was primarily built around the Qt 5.13.2-1 framework. Other dependencies include QuaZIP 0.8.1, zlib 1.2.11, used for zipping and unzipping directories, and Botan 2.12. Since these libraries do not depend on anything besides Qt, they should theoretically compile on any platform that Qt supports.
To update Botan, you must regenerate the amalgamation build from source:

```bash
    # for linux clang
    ./configure.py --cc=clang --amalgamation --disable-shared --disable-modules=pkcs11
    # for linux gcc
    ./configure.py --cc=gcc --amalgamation --disable-shared --disable-modules=pkcs11
    # for Windows MinGW x86
    python configure.py --cpu=x86_64 --cc=gcc --os=mingw --amalgamation --disable-shared --disable-modules=pkcs11
    # for Windows MinGW x32
    python configure.py --cpu=x86_32 --cc=gcc --os=mingw --amalgamation --disable-shared --disable-modules=pkcs11
```

To build the program from source, the appropriate Qt version should be installed and configured for the target platform, including any tools such as Qt Creator.<br>
Arsenic work well but it need some code simplifications, code comments, orthographic corrections ,etc.<br> Any suggestions or help are welcome.

## AVERTISSEMENT: ##
Un logiciel de chiffrage n'est pas magique ou ultime. Si vous utilisez un systeme d'exploitation vulnérable, Arsenic ou même n'importe quel outil cryptographique ne sert à rien. Arsenic ne vous protege pas contre les virus, les keyloggers, l'annalyse de disque dur, le cache des mignatures etc...

## Plus de capture d'ecran: ##

<img src='screenshots/pass_generator.png'/>
<img src='screenshots/hash.png'/>
<img src='screenshots/config.png'/>

## License d'utilisation: ##
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

<img src='https://www.gnu.org/graphics/gplv3-with-text-136x68.png'/>
