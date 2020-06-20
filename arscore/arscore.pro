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

DEFINES += QUAZIP_STATIC

# Botan
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/release/ -lbotan
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/debug/ -lbotan
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/botan/ -lbotan
unix:INCLUDEPATH += $$PWD/../thirdparty/botan/unix/
win32:INCLUDEPATH += $$PWD/../thirdparty/botan/win/

# zxcvbn
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/release/ -lzxcvbn
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/debug/ -lzxcvbn
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/ -lzxcvbn
INCLUDEPATH += $$PWD/../thirdparty/zxcvbn

# Quazip
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/release/ -lQt5Quazip
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/debug/ -lQt5Quazip
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/quazip/ -lQt5Quazip
INCLUDEPATH += $$PWD/../thirdparty/quazip

# Zlib
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zlib/release/ -lzlib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zlib/debug/ -lzlib
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/zlib/ -lzlib
INCLUDEPATH += $$PWD/../thirdparty/zlib

#INSTALL Linux
    target.path = /usr/lib/
    INSTALLS += target
