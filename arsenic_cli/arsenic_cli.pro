TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG += qt
QT       += core
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = arsenic_cli

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
    ../crypto/hash_tool.h
