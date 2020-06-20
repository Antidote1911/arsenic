SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

# Botan
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/release/ -lbotan
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/debug/ -lbotan
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/botan/ -lbotan
unix:INCLUDEPATH += $$PWD/thirdparty/botan/unix/
win32:INCLUDEPATH += $$PWD/thirdparty/botan/win/

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
