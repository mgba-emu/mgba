cmake_minimum_required(VERSION 3.22.1)

project(mgba_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(MGBA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../../.." ABSOLUTE)

option(MGBA_ANDROID_WARNINGS_AS_ERRORS "Treat Android platform native warnings as errors" OFF)
option(MGBA_ANDROID_ENABLE_GDB_STUB "Build Android native core with debugger and GDB stub support" OFF)

add_compile_definitions(IOAPI_NO_64)

set(ENABLE_DEBUGGERS ${MGBA_ANDROID_ENABLE_GDB_STUB} CACHE BOOL "" FORCE)
set(ENABLE_GDB_STUB ${MGBA_ANDROID_ENABLE_GDB_STUB} CACHE BOOL "" FORCE)
set(ENABLE_SCRIPTING OFF CACHE BOOL "" FORCE)
set(BUILD_QT OFF CACHE BOOL "" FORCE)
set(BUILD_SDL OFF CACHE BOOL "" FORCE)
set(BUILD_LIBRETRO OFF CACHE BOOL "" FORCE)
set(BUILD_PERF OFF CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SUITE OFF CACHE BOOL "" FORCE)
set(BUILD_CINEMA OFF CACHE BOOL "" FORCE)
set(BUILD_HEADLESS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_GL OFF CACHE BOOL "" FORCE)
set(BUILD_GLES2 ON CACHE BOOL "" FORCE)
set(BUILD_GLES3 OFF CACHE BOOL "" FORCE)
set(BUILD_LTO OFF CACHE BOOL "" FORCE)
set(USE_EDITLINE OFF CACHE BOOL "" FORCE)
set(USE_FFMPEG OFF CACHE BOOL "" FORCE)
set(USE_DISCORD_RPC OFF CACHE BOOL "" FORCE)
set(USE_EPOXY OFF CACHE STRING "" FORCE)
set(USE_ELF OFF CACHE BOOL "" FORCE)
set(USE_FREETYPE OFF CACHE BOOL "" FORCE)
set(USE_JSON_C OFF CACHE BOOL "" FORCE)
set(USE_LIBZIP OFF CACHE BOOL "" FORCE)
set(USE_LUA OFF CACHE BOOL "" FORCE)
set(USE_MINIZIP OFF CACHE BOOL "" FORCE)
set(USE_SQLITE3 OFF CACHE BOOL "" FORCE)
set(USE_ZLIB ON CACHE BOOL "" FORCE)
set(USE_PNG ON CACHE BOOL "" FORCE)
set(USE_LZMA ON CACHE BOOL "" FORCE)

add_subdirectory("${MGBA_ROOT}" "${CMAKE_BINARY_DIR}/mgba-core")

add_library(mgba-android SHARED
    AndroidAudioOutput.cpp
    AndroidCoreRunner.cpp
    JniUtils.cpp
    mgba_android_jni.cpp
)

target_include_directories(mgba-android
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${MGBA_ROOT}/include"
        "${MGBA_ROOT}/src"
        "${CMAKE_BINARY_DIR}/mgba-core/include"
)

target_compile_definitions(mgba-android
    PRIVATE
        ENABLE_VFS
        ENABLE_VFS_FD
        ENABLE_DIRECTORIES
        $<$<BOOL:${MGBA_ANDROID_ENABLE_GDB_STUB}>:ENABLE_DEBUGGERS>
        $<$<BOOL:${MGBA_ANDROID_ENABLE_GDB_STUB}>:ENABLE_GDB_STUB>
)

if(MGBA_ANDROID_WARNINGS_AS_ERRORS)
    target_compile_options(mgba-android
        PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Werror>
            $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Werror>
    )
endif()

target_link_libraries(mgba-android
    PRIVATE
        mgba
        android
        log
        dl
        EGL
        GLESv2
        OpenSLES
)
