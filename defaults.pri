SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

INCLUDEPATH += $$PWD/arsenic

# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
