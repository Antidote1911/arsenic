TEMPLATE      = subdirs

SUBDIRS = \
    arsenic \
    tests \
    arsenic_cli

# clang
QMAKE_CXX = clang++
QMAKE_LINK = clang++
QMAKE_CC = clang
QMAKE_CXXFLAGS += -std=c++1y
