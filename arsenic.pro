#-------------------------------------------------
#
# Project created by QtCreator 2017-02-23T19:06:31
#
#-------------------------------------------------

TEMPLATE      = subdirs
CONFIG += ordered
QMAKE_CXXFLAGS += -std=c++17

SUBDIRS += src/botan/botan.pro \
           src/zxcvbn/zxcvbn.pro \
           src/tests/tests.pro \
           src/arsenic


linux-g++ {
CONFIG(release, debug|release) {
    message(Building GCC Release (Qt $${QT_VERSION}))}
CONFIG(debug, debug|release) {
    message(Building GCC Debug (Qt $${QT_VERSION}))}
}


linux-clang {
CONFIG(release, debug|release) {
    message(Building clang Release (Qt $${QT_VERSION}))}
CONFIG(debug, debug|release) {
    message(Building clang Debug (Qt $${QT_VERSION}))}
}

DISTFILES += \
    .appveyor.yml \
    .travis.yml \
    LICENSE \
    README.md \
    README_FR.md \
    botan_license.txt \
    screenshots/main_dark.png \
    screenshots/hash.png \
    screenshots/cryptopad_light.png \
    screenshots/pass_gen.png
