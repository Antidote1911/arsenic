SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

DEFINES += QUAZIP_STATIC

# Quazip
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/release/ -lQt5Quazip
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/debug/ -lQt5Quazipd
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/quazip/ -lQt5Quazip

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
