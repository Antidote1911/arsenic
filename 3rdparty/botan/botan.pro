exists($$PWD/botan/configure.py){
    QMAKE_DEBUG: message ( Botan source code is on the disk )
} else {
    QMAKE_DEBUG{
        message ( You need up update your libbotan $$QT_ARCH_TARGET )
        message ( trying now )
    }
    win32-g++{
        message ( win32-g++ )
        system( $$PWD/update-botan.bat )
    }
    win32-msvc{
        message ( win32-msvc )
        system( $$PWD/update-botan.bat )
    }
    linux {
        system( ./update-botan.sh )
    }
    osx{
        system( ./update_botan_osx.sh )
    }
    exists($$PWD/botan/configure.py){
        QMAKE_DEBUG: message( Success botan is now setup )
    } else {
        error(Failed to download and setup botan)
    }
}

TEMPLATE = aux
DISTFILES += \
    $$PWD/update-botan.sh \
    $$PWD/update-botan.bat \
    $$PWD/update_botan_osx.sh

include(botan.pri)
BOTAN_BUILD_DIR = $$OUT_PWD/$$BOTAN_BUILD_DIR
BOTAN_FILE_PATH = $$BOTAN_BUILD_DIR/$$BOTAN_FULL_NAME
BOTAN_BUILD_DIR_FOR_SHELL = $$shell_quote($$shell_path($$BOTAN_BUILD_DIR))
BOTAN_SOURCE_DIR = $$PWD/botan
linux: QMAKE_DISTCLEAN += -r $$BOTAN_SOURCE_DIR $$BOTAN_BUILD_DIR
win32: QMAKE_DISTCLEAN += -r $$BOTAN_SOURCE_DIR $$BOTAN_BUILD_DIR
osx: QMAKE_DISTCLEAN += -r $$BOTAN_SOURCE_DIR $$BOTAN_BUILD_DIR
TARGET = $$BOTAN_LIB_NAME
PRECOMPILED_HEADER =
CONFIG -= qt
CONFIG += staticlib

msvc: BOTAN_CC_TYPE = msvc
clang: BOTAN_CC_TYPE = clang
else: BOTAN_CC_TYPE = gcc

contains(QT_ARCH, i386): BOTAN_ARCH_SWITCH = "--cpu=x86"
contains(QT_ARCH, x86_64): BOTAN_ARCH_SWITCH = "--cpu=x86_64"
contains(QT_ARCH, arm): BOTAN_ARCH_SWITCH = "--cpu=arm"
linux: BOTAN_OS_SWITCH = "--os=linux"

BOTAN_MODULES = aes aead gcm eax chacha20poly1305 serpent sha3 sha3_bmi2 skein keccak whirlpool shake gost_3411 sm3 tiger streebog rmd160 \
                adler32 md4 md5 crc24 crc32 auto_rng argon2 base64 bcrypt bigint sodium entropy \
                hex hash md5 sha1 sha1_sse2 sha1_x86 sha2_32 sha2_32_bmi2 sha2_32_x86 sha2_64 sha2_64_bmi2 \
                simd system_rng sodium

OTHER_FLAGS = --amalgamation --minimized-build  --disable-shared --build-targets="static" \
              --enable-modules=$$join(BOTAN_MODULES,",",,)
win32-g++ {
    message ( win32-g++ )
    BOTAN_OS_SWITCH = "--os=mingw"
    OTHER_FLAGS += --without-stack-protector
}
win32-msvc {
    message ( win32-msvc )
    BOTAN_OS_SWITCH = "--os=Windows"
    OTHER_FLAGS += --without-stack-protector
}

BOTAN_CXX_FLAGS = $$QMAKE_CXXFLAGS
msvc: BOTAN_CXX_FLAGS += /wd4100 /wd4800 /wd4127 /wd4244 /wd4250 /wd4267 /wd4334 /wd4702 /wd4996 \
                         /D_ENABLE_EXTENDED_ALIGNED_STORAGE
else: BOTAN_CXX_FLAGS += -Wno-unused-parameter
macos: BOTAN_CXX_FLAGS += -mmacosx-version-min=$$QMAKE_MACOSX_DEPLOYMENT_TARGET -isysroot $$shell_quote($$QMAKE_MAC_SDK_PATH)
unix | win32-g++: BOTAN_CXX_FLAGS += -fPIC
win32: OTHER_FLAGS += --link-method=hardlink

CONFIG(debug, debug|release) {
    OTHER_FLAGS += --debug-mode
} else {
    win32{
        !win32-g++: BOTAN_CXX_FLAGS += /O2
    } else {
        BOTAN_CXX_FLAGS += -O3
    }
}
!isEmpty(BOTAN_CXX_FLAGS): OTHER_FLAGS += --cxxflags=$$shell_quote($$BOTAN_CXX_FLAGS)
CONFIGURE_FILE_PATH_FOR_SHELL = $$shell_quote($$shell_path($$BOTAN_SOURCE_DIR/configure.py))

configure_inputs = $$BOTAN_SOURCE_DIR/configure.py

configure.input = configure_inputs
configure.output = $$BOTAN_BUILD_DIR/Makefile
configure.variable_out = BOTAN_MAKEFILE

QMAKE_DEBUG{
    message( $$CONFIGURE_FILE_PATH_FOR_SHELL)
    message($$BOTAN_BUILD_DIR_FOR_SHELL)
}


configure.commands = cd $$BOTAN_BUILD_DIR_FOR_SHELL && \
                        python $$CONFIGURE_FILE_PATH_FOR_SHELL \
                        --prefix=$$INSTALL_PREFIX \
                        --bindir=$$INSTALL_BINS \
                        --libdir=$$INSTALL_LIBS \
                        --includedir=$$INSTALL_HEADERS \
                        --docdir=$$INSTALL_PREFIX/doc/botan-2 \
                        --cc=$$BOTAN_CC_TYPE \
                        --cc-bin=$$shell_quote($$QMAKE_CXX) \
                        --without-documentation --without-sphinx --without-pdf --without-rst2man --without-doxygen \
                        $$BOTAN_ARCH_SWITCH $$BOTAN_OS_SWITCH $$OTHER_FLAGS
QMAKE_EXTRA_COMPILERS += configure

make.input = BOTAN_MAKEFILE
make.output = $$BOTAN_FILE_PATH
make.CONFIG += target_predeps
make.commands = cd $$BOTAN_BUILD_DIR_FOR_SHELL && $(MAKE)
QMAKE_EXTRA_COMPILERS += make


#install.commands = cd $$BOTAN_BUILD_DIR_FOR_SHELL && $(MAKE) install
#QMAKE_EXTRA_TARGETS += install
