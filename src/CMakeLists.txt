project(arsenic LANGUAGES CXX)
message(STATUS "Initiating compile using CMake ${CMAKE_VERSION}")


find_package(Qt6 6 REQUIRED COMPONENTS LinguistTools)
set(CMAKE_AUTORCC ON)
qt_standard_project_setup()
set(CMAKE_CXX_STANDARD 20)


list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake)
find_package(
    Botan 3
    REQUIRED
)
# Generate target 'botan_test' with specific modules
# Any Botan module can be listed here
botan_generate(
    arsenic_core
adler32
aead
aes
argon2
argon2fmt
auto_rng
rng
system_rng
pem
sodium
sha1
sha2_32
sha2_64
sha3
skein
keccak
keccak_perm
whirlpool
blake2
shake
gost_28147
gost_3410
gost_3411
sm2
sm3
sm4
threefish_512
twofish
streebog
rmd160
md4
md5
crc24
crc32
chacha
chacha20poly1305
chacha_rng
serpent
ctr
eax
gcm
siv
)

set(app_icon_resource_windows "${CMAKE_SOURCE_DIR}/share/windows/icon.rc")

add_subdirectory(zxcvbn)
set(ZXCVBN_LIBRARIES arsenic_entropy)

# ── BLAKE3 ────────────────────────────────────────────────────────────────────
# Build BLAKE3 manually so we control which SIMD sources are included.
# BLAKE3's own CMakeLists.txt uses CMAKE_SYSTEM_PROCESSOR (the host CPU),
# which breaks macOS cross-compilation from arm64 to x86_64.
include(FetchContent)
FetchContent_Declare(
    blake3
    URL https://github.com/BLAKE3-team/BLAKE3/archive/refs/tags/1.5.4.tar.gz
)
# CMP0169: FetchContent_MakeAvailable would run blake3's own CMakeLists.txt
# and create a 'blake3' target that conflicts with our manual build below.
# We keep the direct-populate pattern and acknowledge the policy explicitly.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(blake3)
if(NOT blake3_POPULATED)
    FetchContent_Populate(blake3)
endif()

# Determine the real target architecture (respects CMAKE_OSX_ARCHITECTURES)
set(_blake3_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _narch)
    if(_narch EQUAL 1)
        list(GET CMAKE_OSX_ARCHITECTURES 0 _blake3_arch)
    endif()
endif()

set(_blake3_sources
    ${blake3_SOURCE_DIR}/c/blake3.c
    ${blake3_SOURCE_DIR}/c/blake3_dispatch.c
    ${blake3_SOURCE_DIR}/c/blake3_portable.c
)
if(_blake3_arch MATCHES "^(aarch64|arm64)$")
    list(APPEND _blake3_sources ${blake3_SOURCE_DIR}/c/blake3_neon.c)
elseif(_blake3_arch MATCHES "^(x86_64|AMD64|i686)$")
    list(APPEND _blake3_sources
        ${blake3_SOURCE_DIR}/c/blake3_sse2.c
        ${blake3_SOURCE_DIR}/c/blake3_sse41.c
        ${blake3_SOURCE_DIR}/c/blake3_avx2.c
        ${blake3_SOURCE_DIR}/c/blake3_avx512.c
    )
    if(MSVC)
        set_source_files_properties(${blake3_SOURCE_DIR}/c/blake3_avx2.c
            PROPERTIES COMPILE_FLAGS "/arch:AVX2")
        set_source_files_properties(${blake3_SOURCE_DIR}/c/blake3_avx512.c
            PROPERTIES COMPILE_FLAGS "/arch:AVX512")
    else()
        set_source_files_properties(${blake3_SOURCE_DIR}/c/blake3_sse2.c
            PROPERTIES COMPILE_FLAGS "-msse2")
        set_source_files_properties(${blake3_SOURCE_DIR}/c/blake3_sse41.c
            PROPERTIES COMPILE_FLAGS "-msse4.1")
        set_source_files_properties(${blake3_SOURCE_DIR}/c/blake3_avx2.c
            PROPERTIES COMPILE_FLAGS "-mavx2")
        set_source_files_properties(${blake3_SOURCE_DIR}/c/blake3_avx512.c
            PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512vl")
    endif()
endif()

add_library(blake3 STATIC ${_blake3_sources})
target_include_directories(blake3 PUBLIC ${blake3_SOURCE_DIR}/c)

add_executable(arsenic
    WIN32 MACOSX_BUNDLE
    aboutDialog.ui
    configDialog.ui
    hashcheckdialog.ui
    mainwindow.ui
    passwordGeneratorDialog.ui
    Config.h
    Delegate.h
    aboutDialog.h
    Clipboard.h
    Clipboard.cpp
    cli.h
    cli.cpp
    configDialog.h
    hashcheckdialog.h
    loghtml.h
    mainwindow.h
    passwordGeneratorDialog.h
    progressbar.h
    Translator.h
    skin/skin.h
    Config.cpp
    Delegate.cpp
    aboutDialog.cpp
    configDialog.cpp
    hashcheckdialog.cpp
    loghtml.cpp
    main.cpp
    mainwindow.cpp
    passwordGeneratorDialog.cpp
    Translator.cpp
    skin/skin.cpp
    rsc.qrc
    arscore/CryptoThread.h
    arscore/PasswordGenerator.h
    arscore/utils.h
    arscore/consts.h
    arscore/messages.h
    arscore/CryptoThread.cpp
    arscore/PasswordGenerator.cpp
    arscore/utils.cpp
    arscore/consts.cpp
    arscore/messages.cpp
    arscore/PasswordHealth.h
    arscore/PasswordHealth.cpp
    arscore/Random.cpp
    arscore/Random.h
    ${app_icon_resource_windows}
)

target_link_libraries(arsenic
    PRIVATE
        Qt::Core
        Qt::Widgets
        arsenic_core
        ${ZXCVBN_LIBRARIES}
        blake3
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # using Visual Studio C++
  target_link_libraries(arsenic PRIVATE advapi32 user32 Ws2_32)
endif()

if(BOTAN_USE_SYSTEM)
    target_compile_definitions(arsenic PRIVATE ARSENIC_BOTAN_IS_SYSTEM)
endif()

install(TARGETS arsenic
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT arsenic
)

if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_SOURCE_DIR}/share/linux/arsenic.desktop
            DESTINATION share/applications
            COMPONENT arsenic)
    install(FILES ${CMAKE_SOURCE_DIR}/src/pixmaps/app.png
            DESTINATION share/icons/hicolor/256x256/apps
            RENAME arsenic.png
            COMPONENT arsenic)
    install(TARGETS arsenic_entropy
            LIBRARY DESTINATION lib COMPONENT arsenic)
    if(NOT BOTAN_USE_SYSTEM)
        install(TARGETS arsenic_core
                LIBRARY DESTINATION lib COMPONENT arsenic)
    endif()
endif()

if(WIN32)
    install(TARGETS arsenic_entropy
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT arsenic)
    if(NOT BOTAN_USE_SYSTEM)
        install(TARGETS arsenic_core
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT arsenic)
    endif()
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_arsenic_crypto
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_crypto.cpp
        arscore/CryptoThread.cpp
        arscore/messages.cpp
        arscore/utils.cpp
        arscore/consts.cpp
    )
    target_include_directories(test_arsenic_crypto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_arsenic_crypto PRIVATE
        Qt6::Core
        Qt6::Test
        arsenic_core
    )
    target_compile_options(test_arsenic_crypto PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
    )
    if(BOTAN_USE_SYSTEM)
        target_compile_definitions(test_arsenic_crypto PRIVATE ARSENIC_BOTAN_IS_SYSTEM)
    endif()

    add_test(NAME CryptoTests COMMAND test_arsenic_crypto)
    # Argon2id prod params : ~2 s/dérivation → timeout généreux
    set_tests_properties(CryptoTests PROPERTIES TIMEOUT 300)
endif()
