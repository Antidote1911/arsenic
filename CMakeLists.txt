cmake_minimum_required(VERSION 3.28)

project(arsenic VERSION 6.0.3)

find_package(Qt6 6 REQUIRED COMPONENTS Core Widgets LinguistTools)
# Set c++ standard for all targets
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)


if (WIN32)
  # Flat install layout: arsenic.exe at prefix root, languages/ next to it
  set(CMAKE_INSTALL_BINDIR ".")

  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    add_compile_options(-Wa,-mbig-obj)
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    # using Intel C++
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # using Visual Studio C++
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/bigobj>)
  endif()
endif (WIN32)

if (UNIX)
    # do something
endif (UNIX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(BUILD_TESTING "Build the test suite" ON)
if(BUILD_TESTING)
    enable_testing()
endif()

add_subdirectory(src)
add_subdirectory(share/translations)

## Flags (Release is the default)
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
endif()

# ── Arch Linux PKGBUILDs (version injectée depuis PROJECT_VERSION) ────────────
configure_file(
    "${CMAKE_SOURCE_DIR}/share/archlinux_pkgbuild/PKGBUILD.in"
    "${CMAKE_SOURCE_DIR}/share/archlinux_pkgbuild/PKGBUILD"
    @ONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/share/archlinux_pkgbuild/PKGBUILD-AppImage.in"
    "${CMAKE_SOURCE_DIR}/share/archlinux_pkgbuild/PKGBUILD-AppImage"
    @ONLY
)

