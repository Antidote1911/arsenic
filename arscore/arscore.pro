include(../defaults.pri)

QT       -= core

TARGET = arscore
TEMPLATE = lib

SRC_DIR = $$PWD

CONFIG += warn_off

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

SOURCES += \
    fileCrypto.cpp \
    tripleencryption.cpp \
    passwordGenerator.cpp \
    utils.cpp \
    textcrypto.cpp \
    messages.cpp

HEADERS += \
    fileCrypto.h \
    tripleencryption.h \
    passwordGenerator.h \
    utils.h \
    textcrypto.h \
    messages.h \
    constants.h

#INSTALL Linux
    target.path = /usr/lib/
    INSTALLS += target
