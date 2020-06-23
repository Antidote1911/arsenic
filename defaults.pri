SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

DEFINES += QUAZIP_STATIC

# Quazip
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/release/ -lQt5Quazip
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/debug/ -lQt5Quazipd
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/quazip/ -lQt5Quazip

INCLUDEPATH += $$PWD/thirdparty/quazip

# Zlib
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zlib/release/ -lzlib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zlib/debug/ -lzlib
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/zlib/ -lzlib

INCLUDEPATH += $$PWD/thirdparty/zlib

# Botan
LIBS += -L$$OUT_PWD/../thirdparty/botan/build/ -lbotan-2
INCLUDEPATH += $$OUT_PWD/../thirdparty/botan/build
DEPENDPATH += $$OUT_PWD/../thirdparty/botan/build

# zxcvbn
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/release/ -lzxcvbn
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/debug/ -lzxcvbn
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/ -lzxcvbn
INCLUDEPATH += $$PWD/thirdparty/zxcvbn

# Catch
LIBS += -L$$OUT_PWD/../thirdparty/catch/build/ -lcatch
INCLUDEPATH += $$PWD/thirdparty/catch

INCLUDEPATH += $$PWD/arsenic

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
