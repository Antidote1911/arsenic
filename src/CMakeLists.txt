project(arsenic LANGUAGES CXX)
message(STATUS "Initiating compile using CMake ${CMAKE_VERSION}")


find_package(Qt6 6 REQUIRED COMPONENTS LinguistTools)
set(CMAKE_AUTORCC ON)
qt_standard_project_setup()
set(CMAKE_CXX_STANDARD 20)


list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake)
find_package(
    Botan 3.7.1
    REQUIRED
)
# Generate target 'botan_test' with specific modules
# Any Botan module can be listed here
botan_generate(
    botan_test
adler32
aead
aes
aes_ni
argon2
argon2_ssse3
argon2fmt
auto_rng
rng
system_rng
pem
sodium
sha1
sha1_armv8
sha1_sse2
sha1_x86
sha2_32
sha2_32_armv8
sha2_32_bmi2
sha2_32_x86
sha2_64
sha2_64_armv8
sha2_64_bmi2
sha3
skein
keccak
keccak_perm
keccak_perm_bmi2
whirlpool
blake2
blake2mac
blake2s
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
set(ZXCVBN_LIBRARIES zxcvbn)

add_executable(arsenic
    WIN32 MACOSX_BUNDLE
    aboutDialog.ui
    argonTests.ui
    configDialog.ui
    hashcheckdialog.ui
    mainwindow.ui
    passwordGeneratorDialog.ui
    Config.h
    Delegate.h
    aboutDialog.h
    argonTests.h
    Clipboard.h
    Clipboard.cpp
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
    argonTests.cpp    
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
    arscore/textcrypto.h
    arscore/triplecryptoengine.h
    arscore/simplecryptoengine.h
    arscore/utils.h
    arscore/consts.h
    arscore/messages.h
    arscore/CryptoThread.cpp
    arscore/PasswordGenerator.cpp
    arscore/textcrypto.cpp
    arscore/triplecryptoengine.cpp
    arscore/simplecryptoengine.cpp
    arscore/utils.cpp
    arscore/consts.cpp
    arscore/messages.cpp
    arscore/PasswordHealth.h
    arscore/PasswordHealth.cpp
    arscore/Random.cpp
    arscore/Random.h
    arscore/Global.h
    ${app_icon_resource_windows}
)

target_link_libraries(arsenic
    PRIVATE
        Qt::Core
        Qt::Widgets
        botan_test
        ${ZXCVBN_LIBRARIES}
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # using Visual Studio C++
  target_link_libraries(arsenic PRIVATE advapi32 user32 Ws2_32)
endif()


install(TARGETS arsenic)
