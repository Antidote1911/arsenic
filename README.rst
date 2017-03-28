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

Find the full feature list below.

Development is coordinated on `GitHub <https://github.com/Antidote1911/Arsenic>`_
suggestions and contributions are welcome.

If you need help you can open an `issue on GitHub
<https://github.com/Antidote1911/Arsenic/issues>`_

Passphrase Derivation
^^^^^^^^^^^^^^^^^^^^^
The passphrase is extended by Argon2 and a random 16 bytes salt to generate a 48 bytes master_key. This output is split in two parts. One for the 32 bytes encryption_key and one for the 16 bytes Initialisation Vector. 

Argon2 is protected from GPU Brute Force Attack and Side Chanel Attack. Arsenic use Argon2id version of the algorithm, 64MiB Memory usage, 4 pass computation and 2 for parrallelism.

Encryption
^^^^^^^^^^^^^^^^^^^^^
You can choose AES, Serpent or ChaCha20.

- AES is the winner of the competition organized by the NIST. He is approved by NSA for *"Top Secret"* classification level of the US governement organizations.
- Serpent is the second finalist. It is more prudent and robust in this conception but AES is faster.
- ChaCha20 is a new generation algorithm. It is extremly robust and fast than AES on standard architecture not optimized for AES.

The output format of the encrypted file is:

.. image:: https://github.com/Antidote1911/Arsenic/blob/master/screenshots/file_format.png
