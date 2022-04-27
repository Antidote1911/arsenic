include(../defaults.pri)

QT -= gui

CONFIG += console
CONFIG -= app_bundle

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        mainclass.cpp

HEADERS += \
    mainclass.h \
    tqdm.h
    
win32-g++ {
    QMAKE_CXXFLAGS += -Wa,-mbig-obj
}


# core
LIBS += -L$$OUT_PWD/../arscore/build/ -larscore
INCLUDEPATH += $$PWD/../arscore
DEPENDPATH += $$OUT_PWD/../arscore


# INSTALL Linux
target.path = /usr/bin/
INSTALLS += target


