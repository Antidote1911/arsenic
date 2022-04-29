include(../defaults.pri)

QT       += core gui
TEMPLATE = app


CONFIG(release, debug|release): TARGET = arsenic_gui
CONFIG(debug, debug | release): TARGET = arsenic_gui_debug

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# no qDebug in release mode
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

win32:RC_ICONS += pixmaps/app.ico

win32-g++ {
    QMAKE_CXXFLAGS += -Wa,-mbig-obj
}


# core
LIBS += -L$$OUT_PWD/../arscore/build/ -larscore
INCLUDEPATH += $$PWD/../arscore
DEPENDPATH += $$PWD/../arscore

# Botan
LIBS += -L$$OUT_PWD/../3rdparty/botan/build/ -lbotan-3
INCLUDEPATH += $$OUT_PWD/../3rdparty/botan/build
DEPENDPATH += $$OUT_PWD/../3rdparty/botan/build

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
    hashcheckdialog.h \
    loghtml.h \
    mainwindow.h \
    passwordGeneratorDialog.h \
    progressbar.h \
    Translator.h \
    skin/skin.h

SOURCES += \
    Config.cpp \
    Delegate.cpp \
    aboutDialog.cpp \
    argonTests.cpp \
    clipboard.cpp \
    configDialog.cpp \
    hashcheckdialog.cpp \
    loghtml.cpp \
    main.cpp \
    mainwindow.cpp \
    passwordGeneratorDialog.cpp \
    Translator.cpp \
    skin/skin.cpp

RESOURCES += \
    rsc.qrc


TRANSLATIONS = languages/arsenic_fr.ts languages/arsenic_en_US.ts

DISTFILES += \
    languages/arsenic_en_US.ts \
    languages/arsenic_fr.qm

#INSTALL Linux
    target.path = /usr/bin/
    INSTALLS += target

#INSTALL Linux desktop launcher
    launcher.files = desktop/arsenic_gui.desktop
    launcher.path = /usr/share/applications/
    INSTALLS += launcher
    icon.files = pixmaps/app.png
    icon.path = /usr/share/arsenic/
    INSTALLS += icon

#INSTALL Linux translations
    i18n.files = $$replace(TRANSLATIONS, .ts, .qm)
    i18n.path = /usr/share/arsenic/languages/
    INSTALLS += i18n

