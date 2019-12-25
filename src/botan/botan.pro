QT       -= core gui

TARGET = botan
TEMPLATE = lib
# Use this for static botan rather than the default dynamic
CONFIG += staticlib
CONFIG += warn_off
CONFIG += c++17
QMAKE_CXXFLAGS += -std=c++17

unix {

        equals(QMAKE_CXX, clang++) {
             message(Botan clang)
             SOURCES += clang/botan_all.cpp \
                        clang/botan_all_aesni_sse2_ssse3.cpp \
                        clang/botan_all_avx2.cpp \
                        clang/botan_all_bmi2.cpp \
                        clang/botan_all_rdrand.cpp \
                        clang/botan_all_rdseed_sse2.cpp \
                        clang/botan_all_sha_sse2_sse41_ssse3.cpp \
                        clang/botan_all_sha_sse2_ssse3.cpp \
                        clang/botan_all_sse2_ssse3.cpp
             
             HEADERS += clang/botan_all.h \
                        clang/botan_all_internal.h
    
                    }
     
        equals(QMAKE_CXX, g++) {
            message(Botan gcc)
            SOURCES += gcc/botan_all.cpp \
                       gcc/botan_all_aesni_sse2_ssse3.cpp \
                       gcc/botan_all_avx2.cpp \
                       gcc/botan_all_bmi2.cpp \
                       gcc/botan_all_rdrand.cpp \
                       gcc/botan_all_rdseed_sse2.cpp \
                       gcc/botan_all_sha_sse2_sse41_ssse3.cpp \
                       gcc/botan_all_sha_sse2_ssse3.cpp \
                       gcc/botan_all_sse2_ssse3.cpp
             
            HEADERS += gcc/botan_all.h \
                       gcc/botan_all_internal.h
            }

}#end linux

win32 {
             SOURCES += win/botan_all.cpp \
                        win/botan_all_aesni_sse2_ssse3.cpp \
                        win/botan_all_avx2.cpp \
                        win/botan_all_rdrand.cpp \
                        win/botan_all_rdseed_sse2.cpp
                        win/botan_all_sse2_ssse3.cpp

             HEADERS += win/botan_all.h \
                        win/botan_all_internal.h


}

QMAKE_CXXFLAGS += -fstack-protector -maes -mpclmul -mssse3 -mavx2
QMAKE_LFLAGS += -fstack-protector

CONFIG += warn_off

