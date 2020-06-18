#-------------------------------------------------
#
# Project created by QtCreator 2017-02-23T19:06:31
#
#-------------------------------------------------

TEMPLATE      = subdirs
CONFIG += ordered
CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

SUBDIRS += thirdparty \
           arsenic \
           tests


DISTFILES += \
    .appveyor.yml \
    .travis.yml \
    .clang-format \
    LICENSE \
    README.md \
    botan_license.txt \
    screenshots/main_dark.png \
    screenshots/hash.png \
    screenshots/cryptopad_light.png \
    screenshots/pass_gen.png
