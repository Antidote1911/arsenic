SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

# core
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../arscore/release/ -larscore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../arscore/debug/ -larscore
else:unix: LIBS += -L$$OUT_PWD/../arscore/ -larscore
INCLUDEPATH += $$PWD/arscore

# Botan
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/release/ -lbotan
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/botan/debug/ -lbotan
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/botan/ -lbotan
unix:INCLUDEPATH += $$PWD/thirdparty/botan/unix/
win32:INCLUDEPATH += $$PWD/thirdparty/botan/win/

# zxcvbn
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/release/ -lzxcvbn
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/debug/ -lzxcvbn
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/ -lzxcvbn
INCLUDEPATH += $$PWD/thirdparty/zxcvbn

# Catch
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/catch/release/ -lcatch
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../thirdparty/catch/debug/ -lcatch
else:unix: LIBS += -L$$OUT_PWD/../thirdparty/catch/ -lcatch
INCLUDEPATH += $$PWD/thirdparty/catch

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
