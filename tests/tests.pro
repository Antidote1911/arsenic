QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = tests

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../arsenic/fileCrypto.cpp \
    ../arsenic/tripleencryption.cpp \
    ../arsenic/messages.cpp \
    ../arsenic/textcrypto.cpp \
    ../arsenic/utils.cpp \
    main.cpp

HEADERS += catch.hpp \
    ../arsenic/constants.h \
    ../arsenic/fileCrypto.h \
    ../arsenic/tripleencryption.h \
    ../arsenic/messages.h \
    ../arsenic/utils.h \
    ../arsenic/textcrypto.h


linux {
    LIBS += -L$$OUT_PWD/../thirdparty/quazip/ -lquazip
    INCLUDEPATH += $$PWD/../thirdparty/quazip/
    DEPENDPATH += $$PWD/../thirdparty/quazip/

    LIBS += -L$$OUT_PWD/../thirdparty/zlib/ -lzlib
    INCLUDEPATH += $$PWD/../thirdparty/zlib/
    DEPENDPATH += $$PWD/../thirdparty/zlib/

    LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/ -lzxcvbn
    INCLUDEPATH += $$PWD/../thirdparty/zxcvbn/
    DEPENDPATH += $$PWD/../thirdparty/zxcvbn/

    LIBS += -L$$OUT_PWD/../thirdparty/botan/ -lbotan
    INCLUDEPATH += $$PWD/../thirdparty/botan/unix/
    DEPENDPATH += $$PWD/../thirdparty/botan/unix/

}

win32-g++ {
    LIBS += -L$$OUT_PWD/../thirdparty/quazip/release/ -lquazip
    INCLUDEPATH += $$PWD/../thirdparty/quazip/
    DEPENDPATH += $$PWD/../thirdparty/quazip/

    LIBS += -L$$OUT_PWD/../thirdparty/zlib/release/ -lzlib
    INCLUDEPATH += $$PWD/../thirdparty/zlib/
    DEPENDPATH += $$PWD/../thirdparty/zlib/

    LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/release/ -lzxcvbn
    INCLUDEPATH += $$PWD/../thirdparty/zxcvbn/
    DEPENDPATH += $$PWD/../thirdparty/zxcvbn/

    LIBS += -L$$OUT_PWD/../thirdparty/botan/release/ -lbotan
    INCLUDEPATH += $$PWD/../thirdparty/botan/win/
    DEPENDPATH += $$PWD/../thirdparty/botan/win/

    LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
}


