#-------------------------------------------------
#
# Project created by QtCreator 2015-03-05T23:12:44
#
#-------------------------------------------------

QT       += core gui
CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17
TEMPLATE = app

CONFIG(release, debug|release): TARGET = arsenic
CONFIG(debug, debug | release): TARGET = arsenicD

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# no qDebug in release mode
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

#LIBS += -lsodium
DEFINES += QUAZIP_STATIC
win32:RC_ICONS += /pixmaps/app.ico

FORMS += \
    aboutDialog.ui \
    argonTests.ui \
    configDialog.ui \
    hashcheckdialog.ui \
    mainwindow.ui \
    passwordGeneratorDialog.ui

HEADERS += \
    Config.h \
    Delegate.h \
    aboutDialog.h \
    argonTests.h \
    clipboard.h \
    configDialog.h \
    constants.h \
    fileCrypto.h \
    hashcheckdialog.h \
    loghtml.h \
    mainwindow.h \
    messages.h \
    passwordGenerator.h \
    passwordGeneratorDialog.h \
    progressbar.h \
    skin.h \
    textcrypto.h \
    tripleencryption.h \
    utils.h

SOURCES += \
    Config.cpp \
    Delegate.cpp \
    aboutDialog.cpp \
    argonTests.cpp \
    clipboard.cpp \
    configDialog.cpp \
    fileCrypto.cpp \
    hashcheckdialog.cpp \
    loghtml.cpp \
    main.cpp \
    mainwindow.cpp \
    messages.cpp \
    passwordGenerator.cpp \
    passwordGeneratorDialog.cpp \
    skin.cpp \
    textcrypto.cpp \
    tripleencryption.cpp \
    utils.cpp

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

TRANSLATIONS = languages/arsenic_fr.ts languages/arsenic_en.ts

DISTFILES += \
    languages/en.svg \
    languages/fr.svg \
    languages/arsenic_en.qm \
    languages/arsenic_fr.qm \

#INSTALL Linux
    target.path = /usr/bin/
    INSTALLS += target

#INSTALL Linux desktop launcher
    launcher.files = desktop/arsenic.desktop
    launcher.path = /usr/share/applications/
    INSTALLS += launcher
    icon.files = pixmaps/app.png
    icon.path = /usr/share/arsenic/
    INSTALLS += icon

#INSTALL Linux translations
    i18n.files = $$replace(TRANSLATIONS, .ts, .qm)
    i18n.path = /usr/share/arsenic/languages/
    INSTALLS += i18n
    flags.files = languages/*.svg
    flags.path = /usr/share/arsenic/languages/
    INSTALLS += flags
