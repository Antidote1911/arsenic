QT       -= core gui

TARGET = botan
TEMPLATE = lib

CONFIG += c++14 warn_off
CONFIG += object_parallel_to_source

linux {
    CONFIG += static              # for static botan rather than the default dynamic
    
    SOURCES += unix/botan_all.cpp
    HEADERS += unix/botan_all.h

}

win32-g++ {
    QMAKE_CXXFLAGS += -Wa,-mbig-obj
    SOURCES += win/botan_all.cpp
    HEADERS += win/botan_all.h
}
