#-------------------------------------------------
#
# Project created by QtCreator 2015-03-05T23:12:44
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

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

#DEFINES += QUAZIP_STATIC
#DEFINES += ZLIB_STATIC
#DEFINES += BOTAN_STATIC

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

unix:!macx: LIBS += -L$$OUT_PWD/../quazip/ -lquazip
INCLUDEPATH += $$PWD/../quazip/
DEPENDPATH += $$PWD/../quazip/
unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../quazip/libquazip.a

unix:!macx: LIBS += -L$$OUT_PWD/../zlib/ -lzlib
INCLUDEPATH += $$PWD/../zlib/
DEPENDPATH += $$PWD/../zlib/
unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../zlib/libzlib.a


equals(QMAKE_CXX, clang++) {
unix:!macx: LIBS += -L$$OUT_PWD/../botan/ -lbotan
INCLUDEPATH += $$PWD/../botan/clang/
DEPENDPATH += $$PWD/../botan/clang/
unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../botan/libbotan.a
}

equals(QMAKE_CXX, g++) {
unix:!macx: LIBS += -L$$OUT_PWD/../botan/ -lbotan
INCLUDEPATH += $$PWD/../botan/gcc/
DEPENDPATH += $$PWD/../botan/gcc/
unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../botan/libbotan.a
}

#unix:!macx: INCLUDEPATH += /usr/include/botan-2
#unix:!macx: LIBS += -L/usr/include -lbotan-2

DISTFILES += \
    ../../LICENSE \
    ../../README.md \
    ../../README_FR.md \
    ../../botan_license.txt \
    ../../build.sh \
    ../../quazip-0.8.1.tar.gz \
    ../../screenshots/config.png \
    ../../screenshots/hash.png \
    ../../screenshots/main.png \
    ../../screenshots/pass_generator.png \
    ../../zlib-1.2.11.tar.gz

