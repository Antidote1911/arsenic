SRC_DIR = $$PWD

CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17




# specific windows
win32:LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
