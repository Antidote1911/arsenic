#-------------------------------------------------
#
# Project created by QtCreator 2015-03-05T23:12:44
#
#-------------------------------------------------

QT       += core gui
CONFIG += c++17

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(release, debug|release): TARGET = arsenic
CONFIG(debug, debug | release): TARGET = arsenicD

TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# no qDebug in release mode
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

#LIBS += -lsodium

DEFINES += QUAZIP_STATIC
DEFINES += ZLIB_STATIC
DEFINES += BOTAN_STATIC

FORMS += \
    AboutDialog.ui \
    argontests.ui \
    config.ui \
    hashcheckdialog.ui \
    myabstractbar.ui \
    mymainwindow.ui \
    passgenerator.ui

HEADERS += \
    AboutDialog.h \
    argontests.h \
    config.h \
    constants.h \
    decrypt.h \
    divers.h \
    encrypt.h \
    hashcheckdialog.h \
    messages.h \
    myabstractbar.h \
    mydecryptbar.h \
    mydecryptthread.h \
    mydirthread.h \
    myencryptbar.h \
    myencryptthread.h \
    myfileinfo.h \
    myfilesystemmodel.h \
    myinterrupt.h \
    myloadbar.h \
    myloadthread.h \
    mymainwindow.h \
    mysavebar.h \
    mysavethread.h \
    passgenerator.h \
    preferences.h \
    progressbar.h

SOURCES += \
    AboutDialog.cpp \
    argontests.cpp \
    config.cpp \
    decrypt.cpp \
    divers.cpp \
    encrypt.cpp \
    hashcheckdialog.cpp \
    main.cpp \
    myabstractbar.cpp \
    mydecryptbar.cpp \
    mydecryptthread.cpp \
    mydirthread.cpp \
    myencryptbar.cpp \
    myencryptthread.cpp \
    myfileinfo.cpp \
    myfilesystemmodel.cpp \
    myloadbar.cpp \
    myloadthread.cpp \
    mymainwindow.cpp \
    mysavebar.cpp \
    mysavethread.cpp \
    passgenerator.cpp \
    progressbar.cpp

RESOURCES += \
    rsc.qrc

linux {
    LIBS += -L$$OUT_PWD/../quazip/ -lquazip
    INCLUDEPATH += $$PWD/../quazip/
    DEPENDPATH += $$PWD/../quazip/
    PRE_TARGETDEPS += $$OUT_PWD/../quazip/libquazip.a

    LIBS += -L$$OUT_PWD/../zlib/ -lzlib
    INCLUDEPATH += $$PWD/../zlib/
    DEPENDPATH += $$PWD/../zlib/
    PRE_TARGETDEPS += $$OUT_PWD/../zlib/libzlib.a


    equals(QMAKE_CXX, clang++)
    {
        LIBS += -L$$OUT_PWD/../botan/ -lbotan
        INCLUDEPATH += $$PWD/../botan/clang/
        DEPENDPATH += $$PWD/../botan/clang/
        PRE_TARGETDEPS += $$OUT_PWD/../botan/libbotan.a
        }

    equals(QMAKE_CXX, g++)
    {
        LIBS += -L$$OUT_PWD/../botan/ -lbotan
        INCLUDEPATH += $$PWD/../botan/gcc/
        DEPENDPATH += $$PWD/../botan/gcc/
        PRE_TARGETDEPS += $$OUT_PWD/../botan/libbotan.a
    }
}

win32 {
    LIBS += -L$$OUT_PWD/../quazip/release/ -lquazip
    INCLUDEPATH += $$PWD/../quazip/
    DEPENDPATH += $$PWD/../quazip/
    PRE_TARGETDEPS += $$OUT_PWD/../quazip/release/libquazip.a

    LIBS += -L$$OUT_PWD/../zlib/release/ -lzlib
    INCLUDEPATH += $$PWD/../zlib/
    DEPENDPATH += $$PWD/../zlib/
    PRE_TARGETDEPS += $$OUT_PWD/../zlib/release/libzlib.a

    LIBS += -L$$OUT_PWD/../botan/release/ -lbotan
    INCLUDEPATH += $$PWD/../botan/win/
    DEPENDPATH += $$PWD/../botan/win/
    PRE_TARGETDEPS += $$OUT_PWD/../botan/release/libbotan.a

}

QMAKE_CXXFLAGS += -fstack-protector
QMAKE_LFLAGS += -fstack-protector
#unix:!macx: INCLUDEPATH += /usr/include/botan-2
#unix:!macx: LIBS += -L/usr/include -lbotan-2



LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
