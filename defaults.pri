
SRC_DIR = $$PWD

DEFINES += QUAZIP_STATIC

linux {
    LIBS += -L$$OUT_PWD/../thirdparty/quazip/ -lquazip
    INCLUDEPATH += $$PWD/thirdparty/quazip/
    # DEPENDPATH += $$PWD/thirdparty/quazip/

    LIBS += -L$$OUT_PWD/../thirdparty/zlib/ -lzlib
    INCLUDEPATH += $$PWD/thirdparty/zlib/
    # DEPENDPATH += $$PWD/thirdparty/zlib/

    LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/ -lzxcvbn
    INCLUDEPATH += $$PWD/thirdparty/zxcvbn/
    # DEPENDPATH += $$PWD/thirdparty/zxcvbn/

    LIBS += -L$$OUT_PWD/../thirdparty/botan/ -lbotan
    INCLUDEPATH += $$PWD/thirdparty/botan/unix/
    # DEPENDPATH += $$PWD/thirdparty/botan/unix/

}

win32-g++ {
    LIBS += -L$$OUT_PWD/../thirdparty/quazip/release/ -lquazip
    INCLUDEPATH += $$PWD/thirdparty/quazip/
    # DEPENDPATH += $$PWD/thirdparty/quazip/

    LIBS += -L$$OUT_PWD/../thirdparty/zlib/release/ -lzlib
    INCLUDEPATH += $$PWD/thirdparty/zlib/
    # DEPENDPATH += $$PWD/thirdparty/zlib/

    LIBS += -L$$OUT_PWD/../thirdparty/zxcvbn/release/ -lzxcvbn
    INCLUDEPATH += $$PWD/thirdparty/zxcvbn/
    # DEPENDPATH += $$PWD/thirdparty/zxcvbn/

    LIBS += -L$$OUT_PWD/../thirdparty/botan/release/ -lbotan
    INCLUDEPATH += $$PWD/thirdparty/botan/win/
    # DEPENDPATH += $$PWD/thirdparty/botan/win/

    LIBS += -ladvapi32 -luser32 -lws2_32 -lpthread
}

