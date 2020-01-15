QT       -= core gui

TARGET = zxcvbn
TEMPLATE = lib

CONFIG += warn_off

HEADERS += \
    dict-src.h \
    zxcvbn.h

SOURCES += \
    zxcvbn.c
