#-------------------------------------------------
#
# Project created by QtCreator 2017-02-23T19:06:31
#
#-------------------------------------------------

TEMPLATE      = subdirs
CONFIG += ordered
QMAKE_CXXFLAGS += -std=c++17

SUBDIRS += src/zlib/zlib.pro \
           src/quazip/quazip.pro \
           src/botan/botan.pro \
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
    CNAME \
    botan_license.txt \
    build_clang.sh \
    build_gcc.sh \
    quazip-0.8.1.tar.gz \
    screenshots/config.png \
    screenshots/hash.png \
    screenshots/main.png \
    screenshots/pass_generator.png \
    zlib-1.2.11.tar.gz \
    build_windows_msvc.cmd \
    src/arsenic/pixmaps/test.jpg
