SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

DEFINES += QUAZIP_STATIC

# Catch
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/catch/release/ -lcatch
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/catch/debug/ -lcatch
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/catch/ -lcatch

INCLUDEPATH += $$PWD/thirdparty/catch

# Quazip
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/release/ -lQt5Quazip
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/quazip/debug/ -lQt5Quazip
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/quazip/ -lQt5Quazip

INCLUDEPATH += $$PWD/thirdparty/quazip

# Zlib
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zlib/release/ -lzlib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zlib/debug/ -lzlib
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/zlib/ -lzlib

INCLUDEPATH += $$PWD/thirdparty/zlib

# zxcvbn
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/release/ -lzxcvbn
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/debug/ -lzxcvbn
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/ -lzxcvbn

INCLUDEPATH += $$PWD/thirdparty/zxcvbn

# Botan
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/release/ -lbotan
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/debug/ -lbotan
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/botan/ -lbotan

unix:INCLUDEPATH += $$PWD/thirdparty/botan/unix/

win32:INCLUDEPATH += $$PWD/thirdparty/botan/win/

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
