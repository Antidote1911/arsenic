include(../defaults.pri)

QT       += core gui
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
    hashcheckdialog.h \
    loghtml.h \
    mainwindow.h \
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
    hashcheckdialog.cpp \
    loghtml.cpp \
    main.cpp \
    mainwindow.cpp \
    passwordGeneratorDialog.cpp \
    skin.cpp

RESOURCES += \
    rsc.qrc


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

