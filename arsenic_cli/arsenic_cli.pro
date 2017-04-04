QT       += core
CONFIG += c++14
QMAKE_CXXFLAGS += -std=c++14
TEMPLATE = app
CONFIG += console


TARGET = arsenic_cli

SOURCES += main.cpp

HEADERS  +=

# Platform-specific configuration


        equals(QMAKE_CXX, clang++) {
             message(Arsenic cli clang x86_64)

             SOURCES += ../arsenic/crypto/botan/clang/botan_all.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_aesni.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_rdrand.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_rdseed.cpp \
                        ../arsenic/crypto/botan/clang/botan_all_ssse3.cpp

             HEADERS += ../arsenic/crypto/botan/clang/botan_all.h \
                        ../arsenic/crypto/botan/clang/botan_all_internal.h

                    }

        equals(QMAKE_CXX, g++) {
            message(Arsenic cli g++ x86_64)

            SOURCES += ../arsenic/crypto/botan/gcc/botan_all.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_aesni.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_rdrand.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_rdseed.cpp \
                       ../arsenic/crypto/botan/gcc/botan_all_ssse3.cpp

            HEADERS += ../arsenic/crypto/botan/gcc/botan_all.h \
                       ../arsenic/crypto/botan/gcc/botan_all_internal.h
            }
