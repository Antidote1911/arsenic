QT       -= core gui

TARGET = arsenic_core
TEMPLATE = lib

CONFIG += c++14 warn_off

linux {
    CONFIG += static              # for static botan rather than the default dynamic
    equals(QMAKE_CXX, clang++)
    {
        SOURCES += linux_clang/botan_all.cpp
        HEADERS += linux_clang/botan_all.h
    }

    equals(QMAKE_CXX, g++)
    {
        SOURCES += linux_gcc/botan_all.cpp
        HEADERS += linux_gcc/botan_all.h
    }

}

win32-g++ {
    QMAKE_CXXFLAGS += -Wa,-mbig-obj
    SOURCES += win_mingw64/botan_all.cpp
    HEADERS += win_mingw64/botan_all.h
}
