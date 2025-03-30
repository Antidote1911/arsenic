cmake_minimum_required(VERSION 3.28)

project(arsenic)

find_package(Qt6 6 REQUIRED COMPONENTS Core Widgets LinguistTools)
# Set c++ standard for all targets
set(CMAKE_CXX_STANDARD 20)


if (WIN32)
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
    add_compile_options(/bigobj)
  endif()
endif (WIN32)

if (UNIX)
    # do something
endif (UNIX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_subdirectory(src)
add_subdirectory(share/translations)

## Flags (Release is the default)
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
endif()

