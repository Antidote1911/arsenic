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
    crypto.h \
    divers.h \
    hashcheckdialog.h \
    loghtml.h \
    mainwindow.h \
    messages.h \
    passwordGenerator.h \
    passwordGeneratorDialog.h \
    progressbar.h \
    skin.h

SOURCES += \
    Config.cpp \
    Delegate.cpp \
    aboutDialog.cpp \
    argonTests.cpp \
    clipboard.cpp \
    configDialog.cpp \
    crypto.cpp \
    divers.cpp \
    hashcheckdialog.cpp \
    loghtml.cpp \
    main.cpp \
    mainwindow.cpp \
    passwordGenerator.cpp \
    passwordGeneratorDialog.cpp \
    skin.cpp

RESOURCES += \
    rsc.qrc

linux {
    LIBS += -L$$OUT_PWD/../zxcvbn/ -lzxcvbn
    INCLUDEPATH += $$PWD/../zxcvbn/
    DEPENDPATH += $$PWD/../zxcvbn/

INCLUDEPATH += /usr/include/botan-2/
DEPENDPATH += /usr/include/botan-2/
LIBS += /usr/lib/libbotan-2.so



}

win32-g++ {
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
