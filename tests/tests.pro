include(../defaults.pri)

QT -= gui

CONFIG += console
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

# core
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../arscore/release/ -larscore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../arscore/debug/ -larscore
else:unix: LIBS += -L$$OUT_PWD/../arscore/ -larscore
INCLUDEPATH += $$PWD/../arscore

# Catch
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/catch/release/ -lcatch
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/catch/debug/ -lcatch
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/catch/ -lcatch
INCLUDEPATH += $$PWD/../thirdparty/catch

# Botan
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/release/ -lbotan
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/debug/ -lbotan
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/botan/ -lbotan
unix:INCLUDEPATH += $$PWD/../thirdparty/botan/unix/
win32:INCLUDEPATH += $$PWD/../thirdparty/botan/win/

SOURCES += \
    main.cpp
