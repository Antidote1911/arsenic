#-------------------------------------------------
#
# Project created by QtCreator 2017-02-23T19:06:31
#
#-------------------------------------------------

TEMPLATE      = subdirs
CONFIG += ordered


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

