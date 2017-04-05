#-------------------------------------------------
#
# Project created by QtCreator 2017-02-23T19:06:31
#
#-------------------------------------------------

QT       += core gui svg
CONFIG += c++14
QMAKE_CXXFLAGS += -std=c++14

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = arsenic
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS


#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp\
        mainwindow.cpp \
    config.cpp \
    about.cpp \
    Delegate.cpp \
    crypto/argonhash.cpp \
    crypto/Crypto.cpp \
    crypto/hash_tool.cpp \
    crypto/argon2/blake2/blake2b.c \
    crypto/argon2/argon2.c \
    crypto/argon2/core.c \
    crypto/argon2/encoding.c \
    crypto/argon2/ref.c \
    crypto/argon2/thread.c

HEADERS  += mainwindow.h \
    config.h \
    about.h \
    Delegate.h \
    preferences/preferences.h \
    preferences/Constants.h \
    crypto/argon2/argon2.h \
    crypto/argonhash.h \
    crypto/Crypto.h \
    crypto/hash_tool.h

# Platform-specific configuration
linux {

        equals(QMAKE_CXX, clang++) {
             message(Arsenic linux clang x86_64)
             
             SOURCES += crypto/botan/clang/botan_all.cpp \
                        crypto/botan/clang/botan_all_aesni.cpp \
                        crypto/botan/clang/botan_all_rdrand.cpp \
                        crypto/botan/clang/botan_all_rdseed.cpp \
                        crypto/botan/clang/botan_all_ssse3.cpp
             
             HEADERS += crypto/botan/clang/botan_all.h \
                        crypto/botan/clang/botan_all_internal.h
    
                    }
     
        equals(QMAKE_CXX, g++) {
            message(Arsenic g++ x86_64)
            
            SOURCES += crypto/botan/gcc/botan_all.cpp \
                       crypto/botan/gcc/botan_all_aesni.cpp \
                       crypto/botan/gcc/botan_all_rdrand.cpp \
                       crypto/botan/gcc/botan_all_rdseed.cpp \
                       crypto/botan/gcc/botan_all_ssse3.cpp
             
            HEADERS += crypto/botan/gcc/botan_all.h \
                       crypto/botan/gcc/botan_all_internal.h
            }
RC_ICONS += images/arsenic.ico
}#end linux
    
win32 {
    message(Windows x64)
            QMAKE_CXXFLAGS += -bigobj
            LIBS += advapi32.lib user32.lib

            SOURCES += crypto/botan/msvc_x64/botan_all.cpp \
                       crypto/botan/msvc_x64/botan_all_aesni.cpp \
                       crypto/botan/msvc_x64/botan_all_rdrand.cpp \
                       crypto/botan/msvc_x64/botan_all_rdseed.cpp \
                       crypto/botan/msvc_x64/botan_all_ssse3.cpp
             
            HEADERS += crypto/botan/msvc_x64/botan_all.h \
                       crypto/botan/msvc_x64/botan_all_internal.h
                       
                       

} #end windows



FORMS    += mainwindow.ui \
    config.ui \
    about.ui

TRANSLATIONS = arsenic_en.ts \
               arsenic_fr.ts

RESOURCES += arsenic.qrc
