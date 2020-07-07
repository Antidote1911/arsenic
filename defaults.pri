SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

DEFINES += QUAZIP_STATIC

# Quazip
LIBS += -L$$OUT_PWD/../3rdparty/quazip/build/ -lQt5Quazip
INCLUDEPATH += $$PWD/3rdparty/quazip

# zxcvbn
LIBS += -L$$OUT_PWD/../3rdparty/zxcvbn/build/ -lzxcvbn
INCLUDEPATH += $$PWD/3rdparty/zxcvbn

# Zlib
LIBS += -L$$OUT_PWD/../3rdparty/zlib//build/ -lzlib
INCLUDEPATH += $$PWD/3rdparty/zlib

# Catch
LIBS += -L$$OUT_PWD/../3rdparty/catch/build/ -lcatch
INCLUDEPATH += $$PWD/3rdparty/catch

# Botan
LIBS += -L$$OUT_PWD/../3rdparty/botan/build/ -lbotan-2
INCLUDEPATH += $$OUT_PWD/../3rdparty/botan/build

# arscore
LIBS += -L$$OUT_PWD/../arscore/build/ -larscore
INCLUDEPATH += $$OUT_PWD/arscore/build

INCLUDEPATH += $$PWD/arsenic
INCLUDEPATH += $$PWD/arscore

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
