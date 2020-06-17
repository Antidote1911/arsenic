QT       -= core gui

TARGET = zxcvbn
TEMPLATE = lib

CONFIG += warn_off
CONFIG += staticlib

HEADERS += \
    dict-src.h \
    zxcvbn.h

SOURCES += \
    zxcvbn.c

#INSTALL Linux
    target.path = /usr/lib/
    INSTALLS += target