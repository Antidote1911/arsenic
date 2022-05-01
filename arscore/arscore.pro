include(../defaults.pri)

QT       -= core

TARGET = arscore
TEMPLATE = lib
DEFINES += EXPORT_LIB

HEADERS += \
    CryptoThread.h \
    dict-src.h \
    libexport.h \
    passwordGenerator.h \
    textcrypto.h \
    triplecryptoengine.h \
    simplecryptoengine.h \
    utils.h \
    consts.h \
    messages.h \
    zxcvbn.h \


SOURCES += \
    CryptoThread.cpp \
    passwordGenerator.cpp \
    textcrypto.cpp \
    triplecryptoengine.cpp \
    simplecryptoengine.cpp \
    utils.cpp \
    consts.cpp \
    messages.cpp \
    zxcvbn.c \

# Botan
LIBS += -L$$OUT_PWD/../3rdparty/botan/build/ -lbotan-3
INCLUDEPATH += $$OUT_PWD/../3rdparty/botan/build
DEPENDPATH += $$OUT_PWD/../3rdparty/botan/build


DESTDIR = build

#INSTALL Linux
target.path = /usr/lib/
INSTALLS += target
