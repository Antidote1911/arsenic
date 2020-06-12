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
    main.cpp

HEADERS += catch.hpp \
    ../arsenic/constants.h \
    ../arsenic/fileCrypto.h \
    ../arsenic/tripleencryption.h \
    ../arsenic/messages.h \
    ../arsenic/textcrypto.h


linux {
    LIBS += -L$$OUT_PWD/../quazip/ -lquazip
    INCLUDEPATH += $$PWD/../quazip/
    DEPENDPATH += $$PWD/../quazip/

    LIBS += -L$$OUT_PWD/../zlib/ -lzlib
    INCLUDEPATH += $$PWD/../zlib/
    DEPENDPATH += $$PWD/../zlib/

    LIBS += -L$$OUT_PWD/../zxcvbn/ -lzxcvbn
    INCLUDEPATH += $$PWD/../zxcvbn/
    DEPENDPATH += $$PWD/../zxcvbn/

    equals(QMAKE_CXX, clang++)
    {
        LIBS += -L$$OUT_PWD/../botan/ -lbotan
        INCLUDEPATH += $$PWD/../botan/linux_clang/
        DEPENDPATH += $$PWD/../botan/linux_clang/
    }

    equals(QMAKE_CXX, g++)
    {
        LIBS += -L$$OUT_PWD/../botan/ -lbotan
        INCLUDEPATH += $$PWD/../botan/linux_gcc/
        DEPENDPATH += $$PWD/../botan/linux_gcc/
    }
}

win32-g++ {
    LIBS += -L$$OUT_PWD/../quazip/release/ -lquazip
    INCLUDEPATH += $$PWD/../quazip/
    DEPENDPATH += $$PWD/../quazip/

    LIBS += -L$$OUT_PWD/../zlib/release/ -lzlib
    INCLUDEPATH += $$PWD/../zlib/
    DEPENDPATH += $$PWD/../zlib/

    LIBS += -L$$OUT_PWD/../zxcvbn/release/ -lzxcvbn
    INCLUDEPATH += $$PWD/../zxcvbn/
    DEPENDPATH += $$PWD/../zxcvbn/

    LIBS += -L$$OUT_PWD/../botan/release/ -lbotan
    INCLUDEPATH += $$PWD/../botan/win_mingw64/
    DEPENDPATH += $$PWD/../botan/win_mingw64/
    LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
}


