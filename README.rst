Arsenic: Strong files encryption
==================================================

Arsenic is a cryptography application written in C++11 with Qt5, the fantastic crypto library `Botan
<https://botan.randombit.net/>`_, and Argon2, the password-hashing function that won the Password Hashing Competition (PHC). It is released under the `GPL-3.0
<https://github.com/Antidote1911/Arsenic/blob/master/LICENSE>`_ license.

**You must check the law about encryption for yor country !**

.. image:: https://github.com/Antidote1911/Arsenic/blob/master/screenshots/screenshot1.jpg

Detailled description
-----------------------
The user can choose to use ChaCha20Poly1305, AES-256/GCM, or Serpent/GCM to encrypt. The decryption routine automatically detect the good Algorithm with the help of a litle header in the encrypted file.
The hash-calculator and the encryption/decryption are optimized for big files.

Development is coordinated on `GitHub <https://github.com/Antidote1911/Arsenic>`_
suggestions and contributions are welcome. For help you can open an `issue on GitHub <https://github.com/Antidote1911/Arsenic/issues>`_

Passphrase Derivation
^^^^^^^^^^^^^^^^^^^^^
The passphrase is extended by Argon2 and a random 16 bytes salt to generate a 48 bytes master_key. This output is split in two parts. One for the 32 bytes encryption_key and one for the 16 bytes Initialisation Vector.
Argon2 is protected from GPU Brute Force Attack and Side Chanel Attack. Arsenic use Argon2id version of the algorithm, 64MiB Memory usage, 4 pass computation and 2 for parrallelism.

Encryption
^^^^^^^^^^
You can choose AES, Serpent or ChaCha20.

- AES is the winner of the competition organized by the NIST in 1997. He is approved by NSA for *"Top Secret"* classification level of the US governement organizations.
- Serpent is the second finalist. It is more prudent and robust in this conception but AES is faster.
- ChaCha20 is a new generation algorithm. It is extremly robust and fast than AES on standard architecture not optimized for AES.

Arsenic provide three formats for writing encrypted data in file.

1. **Raw Data** - No encoding. The file is more compact than Base64 and hexadecimal encoding, but it can' be opened by text editor.

2. **Base64**  - The file can be opened with a text editor. Exellent for send encrypted file by email, forums or printed ! )

3. **Hexadecimal** -same as base64 but produce bigger files



Authentication
^^^^^^^^^^^^^^
All three algorithms use integrity and `authentication. <https://en.wikipedia.org/wiki/Authenticated_encryption>`_ `The GCM (Galois Counter Mode) <https://github.com/Antidote1911/Arsenic/issues>`_ for AES and Serpent, and `Poly1305 <https://github.com/Antidote1911/Arsenic/issues>`_ for Chacha20.

Encrypted File Format
^^^^^^^^^^^^^^^^^^^^^
The output format of the encrypted file is::

 - Header + Version   (checked by decryption routine)
 - Algorithm Name     (for decryption routine)
 - Encoding type      (for decryption routine)
 - 16 bytes Salt      (for Argon2 pass derrivation)
 -
 - 5651651651651516   |
 - 0165598496848161   | encrypted data.
 - 7896825464565515   | writed with the selected
 - 8979846513213313   | encoding.
 - 2165161946546516   |

Recommandations
^^^^^^^^^^^^^^^
Encryption is not magic and don't protect you if your system is not secure. Most operating systems leave behind cleartext of the data you access (swap files, temp files, hibernation files, browser cache, miniatures picture folder.... )

**A good solution is the full-drive encryption.**

It provides the highest level of security and privacy, because all files, including any temporary files that Windows and applications create on the system partition (typically, without your knowledge or consent), hibernation files, swap files, etc., are always permanently encrypted (even when power supply is suddenly interrupted). Windows also records large amounts of potentially sensitive data, such as the names and locations of files you open, applications you run, etc. All such log files and registry entries are always permanently encrypted as well.

**But don't forget:** Full drive encryption don't protect againt a malware infection like a keylogger !
Use a robust open source operating system like **Linux distribution with full disk encryption**.

Never, never trust a proprietary OS or security software !!!

Notes for developpers
---------------------
I am not noob in crypto but i am learning C++. Contributions and suggestions are welcomes.

Thanks to `adolby <https://github.com/adolby>`_ for some portions of code. If you need a good and nice multiplatform file
encryptor that support theming, have a look to `Kryvos <https://github.com/adolby/Kryvos>`_ ! or `here <https://andrewdolby.com/>`_.

I am french, et j'adore boire mon café et manger ma chocolatine en programmant, même
si je ruine mon clavier à chaque fois ! Pour les bugs, veuillez envoyer un rapport à mes chats (Minours, Minette, et Haribo) :)

To build Arsenic, you'll need Qt 5.0 (or later), Botan 2.0, and a C++14 capable compiler (tests ok with GCC and Clang)
You do not need to install Argon2 or Botan library.

For simplify compillation Argon2 original source code is included in the QtCreator project. Botan is included as an amalgamation configurations.

The configuration commands used to generate Botan GCC amalgamation files is::

 ./configure.py --cc=gcc --amalgamation --disable-shared --cc=clang --disable-module threefish_avx2,tls

You can change gcc to clang, and disable all unnecessary modules for reduce compilation time. I have disabled threefish_avx2 because my processor don't like AVX2 instructions.
