SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

# Catch
LIBS += -L$$OUT_PWD/../3rdparty/catch/build/ -lcatch
INCLUDEPATH += $$PWD/3rdparty/catch

# zxcvbn
LIBS += -L$$OUT_PWD/../3rdparty/zxcvbn/build/ -lzxcvbn
INCLUDEPATH += $$PWD/3rdparty/zxcvbn

# Botan
LIBS += -L$$OUT_PWD/../3rdparty/botan/build/ -lbotan-2
INCLUDEPATH += $$OUT_PWD/../3rdparty/botan/build

# arscore
LIBS += -L$$OUT_PWD/../arscore/build/ -larscore
INCLUDEPATH += $$PWD/arscore

INCLUDEPATH += $$PWD/arsenic


# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
