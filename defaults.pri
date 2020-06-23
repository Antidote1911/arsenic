SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

DEFINES += QUAZIP_STATIC

# Quazip
LIBS += -L$$OUT_PWD/../thirdparty/quazip/build/ -lQt5Quazip
INCLUDEPATH += $$PWD/thirdparty/quazip

# zxcvbn
LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/build/ -lzxcvbn
INCLUDEPATH += $$PWD/thirdparty/zxcvbn

# Zlib
LIBS += -L$$OUT_PWD/../thirdparty/zlib//build/ -lzlib
INCLUDEPATH += $$PWD/thirdparty/zlib

# Catch
LIBS += -L$$OUT_PWD/../thirdparty/catch/build/ -lcatch
INCLUDEPATH += $$PWD/thirdparty/catch

# Botan
LIBS += -L$$OUT_PWD/../thirdparty/botan/build/ -lbotan-2
INCLUDEPATH += $$OUT_PWD/../thirdparty/botan/build



INCLUDEPATH += $$PWD/arsenic

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
