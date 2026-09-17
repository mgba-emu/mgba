# Copyright (C) 2026 Ishan
# Android Port component of mGBA.
#
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
#
# This program is distributed without any warranty. See the GNU General Public License for more details.
#

cmake_minimum_required(VERSION 3.22.1)
project(mgba-android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# android studio causes freezes when using CMAKE_SOURCE_DIR
get_filename_component(MGBA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../" ABSOLUTE)


if(NOT EXISTS ${MGBA_DIR}/CMakeLists.txt)
    message(FATAL_ERROR "mGBA not found at ${MGBA_DIR}.")
endif()

set(BUILD_QT OFF CACHE BOOL "" FORCE)
set(BUILD_SDL OFF CACHE BOOL "" FORCE)
set(BUILD_SDL2 OFF CACHE BOOL "" FORCE)
set(BUILD_PERF OFF CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SUITE OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(BUILD_LIBRETRO OFF CACHE BOOL "" FORCE)
set(USE_FFMPEG OFF CACHE BOOL "" FORCE)
set(USE_LIBZIP OFF CACHE BOOL "" FORCE)
set(USE_MINIZIP OFF CACHE BOOL "" FORCE)
set(USE_ZLIB ON CACHE BOOL "" FORCE)
set(USE_SQLITE3 ON CACHE BOOL "" FORCE)
set(USE_DISCORD_RPC OFF CACHE BOOL "" FORCE)
set(USE_EDITLINE OFF CACHE BOOL "" FORCE)
set(USE_GDB_STUB OFF CACHE BOOL "" FORCE)
set(USE_EPOXY OFF CACHE BOOL "" FORCE)      # desktop GL loader does not work in android
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)

add_subdirectory(${MGBA_DIR} mgba_build)

set(MGBA_CORE_TARGET mgba)

add_library(mgba-android SHARED
    jni_bridge.cpp
    core.cpp
    no_intro_parser.cpp
    audio/oboe_audio_player.cpp
)

# very important to avoid definitions mismatch
get_target_property(MGBA_DEFS ${MGBA_CORE_TARGET} COMPILE_DEFINITIONS)
if(MGBA_DEFS)
    target_compile_definitions(mgba-android PRIVATE
            ${MGBA_DEFS}
    )
endif()

target_include_directories(mgba-android PRIVATE
        "${MGBA_DIR}/include"
        "${MGBA_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

find_package(oboe REQUIRED CONFIG)

find_library(log-lib log)
find_library(android-lib android)
find_library(gles-lib GLESv2)
find_library(z-lib z)

target_link_libraries(mgba-android
    ${MGBA_CORE_TARGET}
    ${log-lib}
    ${android-lib}
    ${gles-lib}
    ${z-lib}
    oboe::oboe
)
