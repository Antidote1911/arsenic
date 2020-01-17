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
win32:RC_ICONS += pixmaps/icon.ico

DEFINES += QUAZIP_STATIC
DEFINES += ZLIB_STATIC

FORMS += \
    aboutDialog.ui \
    abstractbarDialog.ui \
    argontests.ui \
    configDialog.ui \
    hashcheckdialog.ui \
    mainwindow.ui \
    passwordGeneratorDialog.ui

HEADERS += \
    Config.h \
    aboutDialog.h \
    abstractbarDialog.h \
    argontests.h \
    clipboard.h \
    configDialog.h \
    constants.h \
    decrypt.h \
    decryptbar.h \
    decryptthread.h \
    dirthread.h \
    divers.h \
    encrypt.h \
    encryptbar.h \
    encryptthread.h \
    fileinfo.h \
    filesystemmodel.h \
    hashcheckdialog.h \
    interrupt.h \
    loadbar.h \
    loadthread.h \
    mainwindow.h \
    messages.h \
    passwordGenerator.h \
    passwordGeneratorDialog.h \
    progressbar.h \
    savebar.h \
    savethread.h \
    skin.h

SOURCES += \
    Config.cpp \
    aboutDialog.cpp \
    abstractbarDialog.cpp \
    argontests.cpp \
    clipboard.cpp \
    configDialog.cpp \
    decrypt.cpp \
    decryptbar.cpp \
    decryptthread.cpp \
    dirthread.cpp \
    divers.cpp \
    encrypt.cpp \
    encryptbar.cpp \
    encryptthread.cpp \
    fileinfo.cpp \
    filesystemmodel.cpp \
    hashcheckdialog.cpp \
    loadbar.cpp \
    loadthread.cpp \
    main.cpp \
    mainwindow.cpp \
    passwordGenerator.cpp \
    passwordGeneratorDialog.cpp \
    progressbar.cpp \
    savebar.cpp \
    savethread.cpp \
    skin.cpp

RESOURCES += \
    rsc.qrc

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
        LIBS += -L$$OUT_PWD/../botan/ -larsenic_core
        INCLUDEPATH += $$PWD/../botan/linux_clang64/
        DEPENDPATH += $$PWD/../botan/linux_clang64/
        }

    equals(QMAKE_CXX, g++)
    {
        LIBS += -L$$OUT_PWD/../botan/ -larsenic_core
        INCLUDEPATH += $$PWD/../botan/linux_gcc64/
        DEPENDPATH += $$PWD/../botan/linux_gcc64/
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

    LIBS += -L$$OUT_PWD/../botan/release/ -larsenic_core
    INCLUDEPATH += $$PWD/../botan/win_mingw64/
    DEPENDPATH += $$PWD/../botan/win_mingw64/

LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread

}

#unix:!macx: INCLUDEPATH += /usr/include/botan-2
#unix:!macx: LIBS += -L/usr/include -lbotan-2

TRANSLATIONS = languages/arsenic_fr.ts languages/arsenic_en.ts

DISTFILES += \
    languages/en.svg \
    languages/fr.svg \
    languages/arsenic_en.qm \
    languages/arsenic_fr.qm \
    pixmaps/icon.png \
    pixmaps/icon.ico \
    pixmaps/test.jpg
