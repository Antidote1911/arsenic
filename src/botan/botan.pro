QT       -= core gui

TARGET = botan
TEMPLATE = lib
# Use this for static botan rather than the default dynamic
CONFIG += c++14 warn_off #static


QMAKE_CXXFLAGS += -Wa,-mbig-obj

SOURCES += win_mingw64/botan_all.cpp
HEADERS += win_mingw64/botan_all.h
