QT       += core
CONFIG += c++14
TEMPLATE = app
CONFIG += console


TARGET = arsenic_cli

SOURCES += main.cpp

HEADERS  +=

# Platform-specific configuration
linux {

        equals(QMAKE_CXX, clang++) {
             message(Arsenic tests linux clang x86_64)

             SOURCES += ../arsenic/crypto/botan/clang/botan_all.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_aesni.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_rdrand.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_rdseed.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_ssse3.cpp

             HEADERS += ../arsenic/crypto/botan/clang/botan_all.h \
                        ../arsenic/crypto/botan/clang/botan_all_internal.h

                    }

        equals(QMAKE_CXX, g++) {
            message(Arsenic tests g++ x86_64)

            SOURCES += ../arsenic/crypto/botan/gcc/botan_all.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_aesni.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_rdrand.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_rdseed.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_ssse3.cpp

            HEADERS += ../arsenic/crypto/botan/gcc/botan_all.h \
                       ../arsenic/crypto/botan/gcc/botan_all_internal.h
            }
}#end linux

win32 {
    message(Windows x64)
            QMAKE_CXXFLAGS += -bigobj
            LIBS += advapi32.lib user32.lib

            SOURCES += ../arsenic/crypto/botan/msvc_x64/botan_all.cpp \
                       ../arsenic/crypto/botan/msvc_x64/botan_all_aesni.cpp \
                       ../arsenic/crypto/botan/msvc_x64/botan_all_rdrand.cpp \
                       ../arsenic/crypto/botan/msvc_x64/botan_all_rdseed.cpp \
                       ../arsenic/crypto/botan/msvc_x64/botan_all_ssse3.cpp

            HEADERS += ../arsenic/crypto/botan/msvc_x64/botan_all.h \
                       ../arsenic/crypto/botan/msvc_x64/botan_all_internal.h

} #end windows
