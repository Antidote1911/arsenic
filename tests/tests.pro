QT += widgets testlib
CONFIG += c++14

INCLUDEPATH += ..

SOURCES += \
    testsarsenic.cpp \
    ../arsenic/crypto/argon2/blake2/blake2b.c \
    ../arsenic/crypto/argon2/argon2.c \
    ../arsenic/crypto/argon2/core.c \
    ../arsenic/crypto/argon2/encoding.c \
    ../arsenic/crypto/argon2/ref.c \
    ../arsenic/crypto/argon2/thread.c \
    ../arsenic/crypto/Crypto.cpp \
    ../arsenic/crypto/hash_tool.cpp \
    ../arsenic/crypto/argonhash.cpp


HEADERS  += \
    ../arsenic/crypto/argon2/argon2.h \
    ../arsenic/crypto/Crypto.h \
    ../arsenic/crypto/hash_tool.h \
    ../arsenic/crypto/argonhash.h

# Platform-specific configuration


        equals(QMAKE_CXX, clang++) {
             message(Arsenic tests clang x86_64)

             SOURCES += ../arsenic/crypto/botan/clang/botan_all.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_aesni.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_rdrand.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_rdseed.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_ssse3.cpp

             HEADERS += ../arsenic/crypto/botan/clang/botan_all.h \
                        ../arsenic/crypto/botan/clang/botan_all_internal.h

                    }

        equals(QMAKE_CXX, g++) {
            message(Arsenic tests g++ x86_64)

            SOURCES += ../arsenic/crypto/botan/gcc/botan_all.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_aesni.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_rdrand.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_rdseed.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_ssse3.cpp

            HEADERS += ../arsenic/crypto/botan/gcc/botan_all.h \
                       ../arsenic/crypto/botan/gcc/botan_all_internal.h
            }

