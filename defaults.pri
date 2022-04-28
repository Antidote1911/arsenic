SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

INCLUDEPATH += $$PWD/arsenic

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread


linux {
    linux-clang {
        message(linux clang)
        QMAKE_CXXFLAGS += -fstack-protector -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -mavx2
        QMAKE_LFLAGS += -fstack-protector
        QMAKE_LFLAGS += -Wl,-rpath,"'\$$ORIGIN'"
        INCLUDEPATH += ../3rdparty/botan/linux_clang64/
        DEPENDPATH += ../3rdparty/botan/linux_clang64/
        HEADERS += \
        ../3rdparty/botan/linux_clang64/botan_all.h
        SOURCES += \
        ../3rdparty/botan/linux_clang64/botan_all.cpp
        }
    linux-g++ {
        message(Linux g++)
        QMAKE_CXXFLAGS += -fstack-protector -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -mavx2
        QMAKE_LFLAGS += -fstack-protector
        QMAKE_LFLAGS += -Wl,-rpath,"'\$$ORIGIN'"
        INCLUDEPATH += ../3rdparty/botan/linux_gcc64/
        DEPENDPATH += ../3rdparty/botan/linux_gcc64/
        HEADERS += \
        ../3rdparty/botan/linux_gcc64/botan_all.h
        SOURCES += \
        ../3rdparty/botan/linux_gcc64/botan_all.cpp
        }
}

win32 {
    message(win 32)
    QMAKE_LFLAGS += -fstack-protector -bigobj
    QMAKE_CXXFLAGS += -bigobj
    LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
    INCLUDEPATH += ../3rdparty/botan/win_msvc64/
    DEPENDPATH += ../3rdparty/botan/win_msvc64/
    HEADERS += \
    ../3rdparty/botan/win_msvc64/botan_all.h
    SOURCES += \
    ../3rdparty/botan/win_msvc64/botan_all.cpp
}

