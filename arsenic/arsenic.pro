#-------------------------------------------------
#
# Project created by QtCreator 2017-02-23T19:06:31
#
#-------------------------------------------------

QT       += core gui
CONFIG += c++14

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = arsenic
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS


#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp\
        mainwindow.cpp \
    crypto/botan/botan_all.cpp \
    crypto/botan/botan_all_aesni.cpp \
    crypto/botan/botan_all_rdrand.cpp \
    crypto/botan/botan_all_rdseed.cpp \
    crypto/botan/botan_all_ssse3.cpp \
    crypto/argon2/blake2/blake2b.c \
    crypto/argon2/argon2.c \
    crypto/argon2/core.c \
    crypto/argon2/encoding.c \
    crypto/argon2/ref.c \
    crypto/argon2/thread.c \
    crypto/argonhash.cpp \
    crypto/Crypto.cpp \
    crypto/hash_tool.cpp \
    config.cpp \
    about.cpp \
    Delegate.cpp

HEADERS  += mainwindow.h \
    crypto/botan/botan_all.h \
    crypto/botan/botan_all_internal.h \
    crypto/argon2/blake2/blake2-impl.h \
    crypto/argon2/blake2/blake2.h \
    crypto/argon2/blake2/blamka-round-opt.h \
    crypto/argon2/blake2/blamka-round-ref.h \
    crypto/argon2/argon2.h \
    crypto/argon2/core.h \
    crypto/argon2/encoding.h \
    crypto/argon2/ref.h \
    crypto/argon2/thread.h \
    crypto/argonhash.h \
    crypto/Crypto.h \
    crypto/hash_tool.h \
    config.h \
    about.h \
    Delegate.h \
    preferences.h \
    Constants.h

FORMS    += mainwindow.ui \
    config.ui \
    about.ui

TRANSLATIONS = arsenic_en.ts \
               arsenic_fr.ts

