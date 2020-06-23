QT       -= core

TARGET = zxcvbn
TEMPLATE = lib

CONFIG += warn_off

HEADERS += \
    dict-src.h \
    zxcvbn.h

SOURCES += \
    zxcvbn.c

DESTDIR = build

#INSTALL Linux
    target.path = /usr/lib/
    INSTALLS += target

