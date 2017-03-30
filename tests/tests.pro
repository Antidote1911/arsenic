QT += core
QT -= gui

CONFIG += c++11

TARGET = tests
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ../crypto/botan/botan_all.cpp \
    ../crypto/botan/botan_all_aesni.cpp \
    ../crypto/botan/botan_all_rdrand.cpp \
    ../crypto/botan/botan_all_rdseed.cpp \
    ../crypto/botan/botan_all_ssse3.cpp \
    ../crypto/argonhash.cpp \
    ../crypto/argon2/blake2/blake2b.c \
    ../crypto/argon2/argon2.c \
    ../crypto/argon2/core.c \
    ../crypto/argon2/encoding.c \
    ../crypto/argon2/ref.c \
    ../crypto/argon2/thread.c

HEADERS  += ../crypto/argon2/argon2.h \
    ../crypto/botan/botan_all.h \
    ../crypto/botan/botan_all_internal.h \
    ../crypto/argonhash.h \
    ../preferences/preferences.h \
    ../preferences/Constants.h

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
