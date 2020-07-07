QT       -= core

TARGET = arscore
TEMPLATE = lib

CONFIG += warn_off
CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

HEADERS += \
    tripleencryption.h \
    utils.h \
    textcrypto.h \
    messages.h \
    consts.h

SOURCES += \
    tripleencryption.cpp \
    utils.cpp \
    textcrypto.cpp \
    messages.cpp \
    consts.cpp

DESTDIR = build

#INSTALL Linux
    target.path = /usr/lib/
    INSTALLS += target


# Botan
LIBS += -L$$OUT_PWD/../3rdparty/botan/build/ -lbotan-2
INCLUDEPATH += $$OUT_PWD/../3rdparty/botan/build

# Zlib
LIBS += -L$$OUT_PWD/../3rdparty/zlib//build/ -lzlib
INCLUDEPATH += $$PWD/../3rdparty/zlib
