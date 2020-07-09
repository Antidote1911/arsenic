QT       -= core

TARGET = arscore
TEMPLATE = lib
DEFINES += EXPORT_LIB

CONFIG += warn_off
CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

HEADERS += \
    libexport.h \
    tripleencryption.h \
    utils.h \
    textcrypto.h \
    messages.h \
    passwordGenerator.h \
    fileCrypto.h \
    zxcvbn.h \
    dict-src.h \
    consts.h

SOURCES += \
    tripleencryption.cpp \
    utils.cpp \
    textcrypto.cpp \
    messages.cpp \
    passwordGenerator.cpp \
    fileCrypto.cpp \
    consts.cpp \
    zxcvbn.c

DESTDIR = build

#INSTALL Linux
    target.path = /usr/lib/
    INSTALLS += target


DEFINES += QUAZIP_STATIC

# Botan
LIBS += -L$$OUT_PWD/../3rdparty/botan/build/ -lbotan-2
INCLUDEPATH += $$OUT_PWD/../3rdparty/botan/build
DEPENDPATH += $$OUT_PWD/../3rdparty/botan/build

# Quazip
LIBS += -L$$OUT_PWD/../3rdparty/quazip/build/ -lQt5Quazip
INCLUDEPATH += $$PWD/../3rdparty/quazip
DEPENDPATH += $$PWD/../3rdparty/quazip

# Zlib
LIBS += -L$$OUT_PWD/../3rdparty/zlib//build/ -lzlib
INCLUDEPATH += $$PWD/../3rdparty/zlib
DEPENDPATH += $$PWD/../3rdparty/zlib

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
