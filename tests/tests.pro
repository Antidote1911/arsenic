QT += core
QT -= gui

CONFIG += c++14

TARGET = tests
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ../arsenic/crypto/botan/botan_all.cpp \
    ../arsenic/crypto/botan/botan_all_aesni.cpp \
    ../arsenic/crypto/botan/botan_all_rdrand.cpp \
    ../arsenic/crypto/botan/botan_all_rdseed.cpp \
    ../arsenic/crypto/botan/botan_all_ssse3.cpp \
    ../arsenic/crypto/argonhash.cpp \
    ../arsenic/crypto/argon2/blake2/blake2b.c \
    ../arsenic/crypto/argon2/argon2.c \
    ../arsenic/crypto/argon2/core.c \
    ../arsenic/crypto/argon2/encoding.c \
    ../arsenic/crypto/argon2/ref.c \
    ../arsenic/crypto/argon2/thread.c

HEADERS  += ../arsenic/crypto/argon2/argon2.h \
    ../arsenic/crypto/botan/botan_all.h \
    ../arsenic/crypto/botan/botan_all_internal.h \
    ../arsenic/crypto/argonhash.h \
    ../arsenic/preferences/preferences.h \
    ../arsenic/preferences/Constants.h

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
