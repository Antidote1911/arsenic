#-------------------------------------------------
#
# Project created by QtCreator 2015-03-05T23:12:44
#
#-------------------------------------------------

QT       += core gui svg
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
    AboutDialog.ui \
    PasswordGeneratorWidget.ui \
    argontests.ui \
    conf.ui \
    hashcheckdialog.ui \
    myabstractbar.ui \
    mymainwindow.ui

HEADERS += \
    AboutDialog.h \
    Clipboard.h \
    Config.h \
    PasswordGenerator.h \
    PasswordGeneratorWidget.h \
    argontests.h \
    conf.h \
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
    progressbar.h \
    skin.h

SOURCES += \
    AboutDialog.cpp \
    Clipboard.cpp \
    Config.cpp \
    PasswordGenerator.cpp \
    PasswordGeneratorWidget.cpp \
    argontests.cpp \
    conf.cpp \
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
    progressbar.cpp \
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
