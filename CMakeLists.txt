cmake_minimum_required(VERSION 2.6)
project(mGBA C)
set(BINARY_NAME mgba CACHE INTERNAL "Name of output binaries")
if(NOT MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=c99")
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS /wd4003 /wd4244 /wd4146")
endif()
set(USE_CLI_DEBUGGER ON CACHE BOOL "Whether or not to enable the CLI-mode ARM debugger")
set(USE_GDB_STUB ON CACHE BOOL "Whether or not to enable the GDB stub ARM debugger")
set(USE_FFMPEG ON CACHE BOOL "Whether or not to enable FFmpeg support")
set(USE_PNG ON CACHE BOOL "Whether or not to enable PNG support")
set(USE_LIBZIP ON CACHE BOOL "Whether or not to enable ZIP support")
set(USE_MAGICK ON CACHE BOOL "Whether or not to enable ImageMagick support")
set(USE_BLIP ON CACHE BOOL "Whether or not to enable blip_buf support")
set(USE_LZMA ON CACHE BOOL "Whether or not to enable 7-Zip support")
set(BUILD_QT ON CACHE BOOL "Build Qt frontend")
set(BUILD_SDL ON CACHE BOOL "Build SDL frontend")
set(BUILD_LIBRETRO OFF CACHE BOOL "Build libretro core")
set(BUILD_PERF OFF CACHE BOOL "Build performance profiling tool")
set(BUILD_STATIC OFF CACHE BOOL "Build a static library")
set(BUILD_SHARED ON CACHE BOOL "Build a shared library")
set(BUILD_GL ON CACHE STRING "Build with OpenGL")
set(BUILD_GLES2 OFF CACHE STRING "Build with OpenGL|ES 2")
file(GLOB ARM_SRC ${CMAKE_SOURCE_DIR}/src/arm/*.c)
file(GLOB GBA_SRC ${CMAKE_SOURCE_DIR}/src/gba/*.c)
file(GLOB GBA_CHEATS_SRC ${CMAKE_SOURCE_DIR}/src/gba/cheats/*.c)
file(GLOB GBA_RR_SRC ${CMAKE_SOURCE_DIR}/src/gba/rr/*.c)
file(GLOB GBA_SV_SRC ${CMAKE_SOURCE_DIR}/src/gba/supervisor/*.c)
file(GLOB UTIL_SRC ${CMAKE_SOURCE_DIR}/src/util/*.[cSs])
file(GLOB GUI_SRC ${CMAKE_SOURCE_DIR}/src/util/gui/*.c)
file(GLOB RENDERER_SRC ${CMAKE_SOURCE_DIR}/src/gba/renderers/*.c)
file(GLOB SIO_SRC ${CMAKE_SOURCE_DIR}/src/gba/sio/lockstep.c)
file(GLOB THIRD_PARTY_SRC ${CMAKE_SOURCE_DIR}/src/third-party/inih/*.c)
list(APPEND UTIL_SRC ${CMAKE_SOURCE_DIR}/src/platform/commandline.c)
set(VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-mem.c)
source_group("ARM core" FILES ${ARM_SRC})
source_group("GBA board" FILES ${GBA_SRC} ${RENDERER_SRC} ${SIO_SRC})
source_group("GBA supervisor" FILES ${GBA_CHEATS_SRC} ${GBA_SV_SRC} ${GBA_RR_SRC})
source_group("Utilities" FILES ${UTIL_SRC})
include_directories(${CMAKE_SOURCE_DIR}/src/arm)
include_directories(${CMAKE_SOURCE_DIR}/src)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (e.g. Release or Debug)" FORCE)
endif()

if (NOT DEFINED LIBDIR)
	set(LIBDIR "lib")
endif()

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIBDIR}")

include(GNUInstallDirs)

# Function definitions
include(FindPkgConfig)
function(find_feature FEATURE_NAME FEATURE_REQUIRES)
	if (NOT ${FEATURE_NAME})
		return()
	endif()
	foreach(REQUIRE ${FEATURE_REQUIRES})
		if(NOT ${REQUIRE}_FOUND)
			find_package(${REQUIRE} QUIET)
			if(NOT ${REQUIRE}_FOUND)
				pkg_search_module(${REQUIRE} ${REQUIRE})
				if (NOT ${REQUIRE}_FOUND)
					message(WARNING "Requested module ${REQUIRE} missing for feature ${FEATURE_NAME}. Feature disabled.")
					set(${FEATURE_NAME} OFF PARENT_SCOPE)
					return()
				endif()
			endif()
		endif()
		string(TOUPPER ${REQUIRE} UREQUIRE)
		set(${UREQUIRE}_CFLAGS_OTHER ${${REQUIRE}_CFLAGS_OTHER} PARENT_SCOPE)
		set(${UREQUIRE}_FOUND ${${REQUIRE}_FOUND} PARENT_SCOPE)
		set(${UREQUIRE}_INCLUDE_DIRS ${${REQUIRE}_INCLUDE_DIRS} PARENT_SCOPE)
		set(${UREQUIRE}_LIBRARIES ${${REQUIRE}_LIBRARIES} PARENT_SCOPE)
		set(${UREQUIRE}_LIBRARY_DIRS ${${REQUIRE}_LIBRARY_DIRS} PARENT_SCOPE)
		set(${UREQUIRE}_LDFLAGS_OTHER ${${REQUIRE}_LDFLAGS_OTHER} PARENT_SCOPE)
	endforeach()
endfunction()

# Version information
add_custom_target(version-info ALL ${CMAKE_COMMAND} -E touch ${CMAKE_SOURCE_DIR}/src/util/version.c.in
	COMMAND ${CMAKE_COMMAND}
	-DBINARY_NAME=${BINARY_NAME}
	-DCONFIG_FILE=${CMAKE_SOURCE_DIR}/src/util/version.c.in
	-DOUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/version.c
	-P ${CMAKE_SOURCE_DIR}/version.cmake
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/version.cmake)
configure_file(${CMAKE_SOURCE_DIR}/src/util/version.c.in ${CMAKE_CURRENT_BINARY_DIR}/version.c)
list(APPEND UTIL_SRC ${CMAKE_BINARY_DIR}/version.c)
source_group("Generated sources" FILES ${CMAKE_BINARY_DIR}/version.c)

# Advanced settings
set(BUILD_LTO ON CACHE BOOL "Build with link-time optimization")
set(BUILD_PGO OFF CACHE BOOL "Build with profiling-guided optimization")
set(PGO_STAGE_2 CACHE BOOL "Rebuild for profiling-guided optimization after profiles have been generated")
set(PGO_DIR "/tmp/gba-pgo/" CACHE PATH "Profiling-guided optimization profiles path")
mark_as_advanced(BUILD_LTO BUILD_PGO PGO_STAGE_2 PGO_DIR)
set(PGO_PRE_FLAGS "-pg -fprofile-generate=${PGO_DIR} -fprofile-arcs")
set(PGO_POST_FLAGS "-fprofile-use=${PGO_DIR} -fbranch-probabilities")

if(BUILD_PGO AND NOT PGO_STAGE_2)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${PGO_PRE_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PGO_PRE_FLAGS}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${PGO_PRE_FLAGS}")
elseif(BUILD_PGO AND PGO_STAGE_2)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${PGO_POST_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PGO_POST_FLAGS}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${PGO_POST_FLAGS}")
endif()

# Feature dependencies
set(FEATURES)
if(CMAKE_SYSTEM_NAME MATCHES .*BSD)
	set(LIBEDIT_LIBRARIES -ledit)
	if (CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
		list(APPEND LIBEDIT_LIBRARIES -ltermcap)
	endif()
else()
	find_feature(USE_CLI_DEBUGGER "libedit")
endif()
if(BUILD_GL)
	find_package(OpenGL QUIET)
	if(NOT OPENGL_FOUND)
		set(BUILD_GL OFF CACHE BOOL "OpenGL not found" FORCE)
	endif()
endif()
if(BUILD_GLES2 AND NOT BUILD_RASPI)
	find_path(OPENGLES2_INCLUDE_DIR NAMES GLES2/gl2.h)
	find_library(OPENGLES2_LIBRARY NAMES GLESv2 GLESv2_CM)
	if(NOT OPENGLES2_INCLUDE_DIR OR NOT OPENGLES2_LIBRARY)
		set(BUILD_GLES2 OFF CACHE BOOL "OpenGL|ES 2 not found" FORCE)
	endif()
endif()
find_feature(USE_FFMPEG "libavcodec;libavformat;libavresample;libavutil;libswscale")
find_feature(USE_PNG "ZLIB;PNG")
find_feature(USE_LIBZIP "libzip")
find_feature(USE_MAGICK "MagickWand")

# Platform support
if(WIN32)
	set(WIN32_VERSION "${LIB_VERSION_MAJOR},${LIB_VERSION_MINOR},${LIB_VERSION_PATCH}")
	add_definitions(-D_WIN32_WINNT=0x0600)
	list(APPEND OS_LIB ws2_32 shlwapi)
	list(APPEND VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-fd.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c)
	file(GLOB OS_SRC ${CMAKE_SOURCE_DIR}/src/platform/windows/*.c)
	source_group("Windows-specific code" FILES ${OS_SRC})
elseif(UNIX)
	add_definitions(-DUSE_PTHREADS)

	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		add_definitions(-D_GNU_SOURCE)
	endif()
	if(NOT APPLE AND NOT HAIKU)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
	endif()

	list(APPEND VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-fd.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c)
	file(GLOB OS_SRC ${CMAKE_SOURCE_DIR}/src/platform/posix/*.c)
	source_group("POSIX-specific code" FILES ${OS_SRC})
elseif(3DS)
	add_definitions(-DCOLOR_16_BIT -DCOLOR_5_6_5)
	list(APPEND OS_LIB ctru)
	file(GLOB OS_SRC ${CMAKE_SOURCE_DIR}/src/platform/3ds/3ds-*.c)
	source_group("3DS-specific code" FILES ${OS_SRC})
elseif(WII)
	list(APPEND VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-file.c)
	add_definitions(-DCOLOR_16_BIT -DCOLOR_5_6_5 -DUSE_VFS_FILE)
	file(GLOB OS_SRC ${CMAKE_SOURCE_DIR}/src/platform/wii/wii-*.c)
	list(APPEND OS_LIB fat ogc)
	source_group("Wii-specific code" FILES ${OS_SRC})
endif()

if(APPLE)
	add_definitions(-D_DARWIN_C_SOURCE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.6")
endif()

if(NOT HAIKU AND NOT MSVC AND NOT PSP2)
	list(APPEND OS_LIB m)
endif()

if(APPLE OR CMAKE_C_COMPILER_ID STREQUAL "GNU" AND BUILD_LTO)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
endif()

if(BUILD_BBB OR BUILD_RASPI OR BUILD_PANDORA)
	if(NOT BUILD_EGL)
		add_definitions(-DCOLOR_16_BIT -DCOLOR_5_6_5)
	endif()
endif()

if(WII)
	add_definitions(-U__STRICT_ANSI__)
	add_executable(${BINARY_NAME}.elf ${CMAKE_SOURCE_DIR}/src/platform/wii/main.c)
	target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${OS_LIB})
	add_custom_command(TARGET ${BINARY_NAME}.elf POST_BUILD COMMAND ${ELF2DOL} ${BINARY_NAME}.elf ${BINARY_NAME}.dol)
endif()

if(BUILD_RASPI)
	set(BUILD_GL OFF CACHE BOOL "OpenGL not supported" FORCE)
endif()

if(BUILD_PANDORA)
	add_definitions(-DBUILD_PANDORA)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*")
	enable_language(ASM)
endif()

if(PSP2)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format")
endif()

include(CheckFunctionExists)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strndup HAVE_STRNDUP)
check_function_exists(snprintf_l HAVE_SNPRINTF_L)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# The strtof_l on Linux not actually exposed nor actually strtof_l
	set(HAVE_STRTOF_L OFF)
else()
	check_function_exists(strtof_l HAVE_STRTOF_L)
endif()
check_function_exists(newlocale HAVE_NEWLOCALE)
check_function_exists(freelocale HAVE_FREELOCALE)
check_function_exists(uselocale HAVE_USELOCALE)
check_function_exists(setlocale HAVE_SETLOCALE)

if(HAVE_STRDUP)
	add_definitions(-DHAVE_STRDUP)
endif()

if(HAVE_STRNDUP)
	add_definitions(-DHAVE_STRNDUP)
endif()

if(HAVE_NEWLOCALE AND HAVE_FREELOCALE AND HAVE_USELOCALE)
	add_definitions(-DHAVE_LOCALE)
	if (HAVE_STRTOF_L)
		add_definitions(-DHAVE_STRTOF_L)
	endif()
	if (HAVE_SNPRINTF_L)
		add_definitions(-DHAVE_SNPRINTF_L)
	endif()
endif()

if(HAVE_SETLOCALE)
	add_definitions(-DHAVE_SETLOCALE)
endif()

# Features
set(DEBUGGER_SRC ${CMAKE_SOURCE_DIR}/src/debugger/debugger.c ${CMAKE_SOURCE_DIR}/src/debugger/memory-debugger.c)
set(FEATURE_SRC)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")

if(USE_CLI_DEBUGGER)
	list(APPEND FEATURES CLI_DEBUGGER)
	list(APPEND FEATURE_SRC ${CMAKE_SOURCE_DIR}/src/debugger/cli-debugger.c)
	list(APPEND FEATURE_SRC ${CMAKE_SOURCE_DIR}/src/debugger/parser.c)
	list(APPEND FEATURE_SRC ${CMAKE_SOURCE_DIR}/src/gba/supervisor/cli.c)
	include_directories(AFTER ${LIBEDIT_INCLUDE_DIRS})
	link_directories(${LIBEDIT_LIBRARY_DIRS})
	set(DEBUGGER_LIB ${LIBEDIT_LIBRARIES})
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libedit2")
else()
	set(DEBUGGER_LIB "")
endif()

if(USE_GDB_STUB)
	list(APPEND FEATURES GDB_STUB)
	list(APPEND DEBUGGER_SRC ${CMAKE_SOURCE_DIR}/src/debugger/gdb-stub.c)
endif()
source_group("ARM debugger" FILES ${DEBUGGER_SRC})

if(USE_FFMPEG)
	list(APPEND FEATURES FFMPEG)
	pkg_search_module(LIBSWRESAMPLE QUIET libswresample)
	if(NOT LIBSWRESAMPLE_FOUND)
		list(APPEND FEATURES LIBAV)
	endif()
	include_directories(AFTER ${LIBAVCODEC_INCLUDE_DIRS} ${LIBAVFORMAT_INCLUDE_DIRS} ${LIBAVRESAMPLE_INCLUDE_DIRS} ${LIBAVUTIL_INCLUDE_DIRS} ${LIBSWSCALE_INCLUDE_DIRS})
	link_directories(${LIBAVCODEC_LIBRARY_DIRS} ${LIBAVFORMAT_LIBRARY_DIRS} ${LIBAVRESAMPLE_LIBRARY_DIRS} ${LIBAVUTIL_LIBRARY_DIRS} ${LIBSWSCALE_LIBRARY_DIRS})
	list(APPEND FEATURE_SRC "${CMAKE_SOURCE_DIR}/src/platform/ffmpeg/ffmpeg-encoder.c")
	string(REGEX MATCH "^[0-9]+" LIBAVCODEC_VERSION_MAJOR ${libavcodec_VERSION})
	string(REGEX MATCH "^[0-9]+" LIBAVFORMAT_VERSION_MAJOR ${libavformat_VERSION})
	string(REGEX MATCH "^[0-9]+" LIBAVRESAMPLE_VERSION_MAJOR ${libavresample_VERSION})
	string(REGEX MATCH "^[0-9]+" LIBAVUTIL_VERSION_MAJOR ${libavutil_VERSION})
	string(REGEX MATCH "^[0-9]+" LIBSWSCALE_VERSION_MAJOR ${libswscale_VERSION})
	list(APPEND DEPENDENCY_LIB ${LIBAVCODEC_LIBRARIES} ${LIBAVFORMAT_LIBRARIES} ${LIBAVRESAMPLE_LIBRARIES} ${LIBAVUTIL_LIBRARIES} ${LIBSWSCALE_LIBRARIES})
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libavcodec${LIBAVCODEC_VERSION_MAJOR}|libavcodec-extra-${LIBAVCODEC_VERSION_MAJOR},libavformat${LIBAVFORMAT_VERSION_MAJOR},libavresample${LIBAVRESAMPLE_VERSION_MAJOR},libavutil${LIBAVUTIL_VERSION_MAJOR},libswscale${LIBSWSCALE_VERSION_MAJOR}")
	set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libavcodec-extra")
	if(APPLE)
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -framework VideoDecodeAcceleration -framework CoreVideo")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework VideoDecodeAcceleration -framework CoreVideo")
	endif()
endif()

if(USE_BLIP)
	list(APPEND THIRD_PARTY_SRC "${CMAKE_SOURCE_DIR}/src/third-party/blip_buf/blip_buf.c")
	add_definitions(-DRESAMPLE_LIBRARY=RESAMPLE_BLIP_BUF)
else()
	add_definitions(-DRESAMPLE_LIBRARY=RESAMPLE_NN)
endif()

if(USE_MAGICK)
	list(APPEND FEATURES MAGICK)
	include_directories(AFTER ${MAGICKWAND_INCLUDE_DIRS})
	link_directories(${MAGICKWAND_LIBRARY_DIRS})
	list(APPEND FEATURE_SRC "${CMAKE_SOURCE_DIR}/src/platform/imagemagick/imagemagick-gif-encoder.c")
	list(APPEND DEPENDENCY_LIB ${MAGICKWAND_LIBRARIES})
	string(REGEX MATCH "^[0-9]+\\.[0-9]+" MAGICKWAND_VERSION_PARTIAL ${MagickWand_VERSION})
	if(${MAGICKWAND_VERSION_PARTIAL} EQUAL "6.7")
		set(MAGICKWAND_DEB_VERSION "5")
	else()
		set(MAGICKWAND_DEB_VERSION "-6.q16-2")
	endif()
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libmagickwand${MAGICKWAND_DEB_VERSION}")
endif()

if(USE_PNG)
	list(APPEND FEATURES PNG)
	include_directories(AFTER ${PNG_INCLUDE_DIRS})
	list(APPEND DEPENDENCY_LIB ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libpng12-0,zlib1g")
endif()

if(USE_LIBZIP)
	include_directories(AFTER ${LIBZIP_INCLUDE_DIRS})
	link_directories(${LIBZIP_LIBRARY_DIRS})
	list(APPEND DEPENDENCY_LIB ${LIBZIP_LIBRARIES})
	list(APPEND FEATURES LIBZIP)
	list(APPEND VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-zip.c)
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libzip2")
endif()

if (USE_LZMA)
	include_directories(AFTER ${CMAKE_SOURCE_DIR}/third-party/lzma)
	add_definitions(-D_7ZIP_PPMD_SUPPPORT)
	list(APPEND VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-lzma.c)
	set(LZMA_SRC
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zAlloc.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zArcIn.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zBuf.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zBuf2.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zCrc.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zCrcOpt.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zDec.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/CpuArch.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/LzmaDec.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/Lzma2Dec.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/Bra.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/Bra86.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/Bcj2.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/Ppmd7.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/Ppmd7Dec.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zFile.c
		${CMAKE_SOURCE_DIR}/src/third-party/lzma/7zStream.c)
	list(APPEND FEATURE_SRC ${LZMA_SRC})
	list(APPEND FEATURES LZMA)
endif()

set(FEATURE_DEFINES)
foreach(FEATURE IN LISTS FEATURES)
	list(APPEND FEATURE_DEFINES "USE_${FEATURE}")
endforeach()

source_group("Virtual files" FILES ${VFS_SRC})
source_group("Extra features" FILES ${FEATURE_SRC})
source_group("Third-party code" FILES ${THIRD_PARTY_SRC})

# Binaries
set(CORE_SRC
	${ARM_SRC}
	${GBA_SRC}
	${GBA_CHEATS_SRC}
	${GBA_RR_SRC}
	${GBA_SV_SRC}
	${DEBUGGER_SRC}
	${RENDERER_SRC}
	${SIO_SRC}
	${UTIL_SRC}
	${VFS_SRC}
	${OS_SRC}
	${THIRD_PARTY_SRC})

set(SRC
	${CORE_SRC}
	${FEATURE_SRC})

if(NOT BUILD_STATIC AND NOT BUILD_SHARED)
	set(BUILD_SHARED ON)
endif()

if(BUILD_SHARED)
	add_library(${BINARY_NAME} SHARED ${SRC})
	set_target_properties(${BINARY_NAME} PROPERTIES SOVERSION ${LIB_VERSION_ABI})
	if(BUILD_STATIC)
		add_library(${BINARY_NAME}-static STATIC ${SRC})
		set_target_properties(${BINARY_NAME}-static PROPERTIES COMPILE_DEFINITIONS "${FEATURE_DEFINES}")
		install(TARGETS ${BINARY_NAME}-static DESTINATION ${LIBDIR} COMPONENT lib${BINARY_NAME})
		add_dependencies(${BINARY_NAME}-static version-info)
	endif()
else()
	add_library(${BINARY_NAME} STATIC ${SRC})
endif()

add_dependencies(${BINARY_NAME} version-info)

target_link_libraries(${BINARY_NAME} ${DEBUGGER_LIB} ${DEPENDENCY_LIB} ${OS_LIB})
install(TARGETS ${BINARY_NAME} LIBRARY DESTINATION ${LIBDIR} COMPONENT lib${BINARY_NAME} NAMELINK_SKIP ARCHIVE DESTINATION ${LIBDIR} RUNTIME DESTINATION ${LIBDIR} COMPONENT lib${BINARY_NAME})
if(UNIX AND NOT APPLE)
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-16.png DESTINATION share/icons/hicolor/16x16/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-24.png DESTINATION share/icons/hicolor/24x24/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-32.png DESTINATION share/icons/hicolor/32x32/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-48.png DESTINATION share/icons/hicolor/48x48/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-64.png DESTINATION share/icons/hicolor/64x64/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-96.png DESTINATION share/icons/hicolor/96x96/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-128.png DESTINATION share/icons/hicolor/128x128/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-256.png DESTINATION share/icons/hicolor/256x256/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-512.png DESTINATION share/icons/hicolor/512x512/apps RENAME mgba.png COMPONENT lib${BINARY_NAME})
endif()
set_target_properties(${BINARY_NAME} PROPERTIES VERSION ${LIB_VERSION_STRING} SOVERSION ${LIB_VERSION_ABI} COMPILE_DEFINITIONS "${FEATURE_DEFINES}")

if(BUILD_GL)
	add_definitions(-DBUILD_GL)
endif()

if(BUILD_GLES2)
	add_definitions(-DBUILD_GLES2)
endif()

if(BUILD_LIBRETRO)
	file(GLOB RETRO_SRC ${CMAKE_SOURCE_DIR}/src/platform/libretro/*.c)
	add_library(${BINARY_NAME}_libretro SHARED ${CORE_SRC} ${RETRO_SRC})
	set_target_properties(${BINARY_NAME}_libretro PROPERTIES PREFIX "" COMPILE_DEFINITIONS "COLOR_16_BIT;COLOR_5_6_5")
	target_link_libraries(${BINARY_NAME}_libretro ${OS_LIB})
endif()

if(BUILD_SDL)
	add_definitions(-DBUILD_SDL)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/platform/sdl ${CMAKE_BINARY_DIR}/sdl)
endif()

if(BUILD_QT)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/platform/qt ${CMAKE_BINARY_DIR}/qt)
endif()

if(BUILD_PERF)
	set(PERF_SRC ${CMAKE_SOURCE_DIR}/src/platform/perf-main.c)
	if(UNIX AND NOT APPLE)
		list(APPEND PERF_LIB rt)
	endif()

	add_executable(${BINARY_NAME}-perf ${PERF_SRC})
	target_link_libraries(${BINARY_NAME}-perf ${BINARY_NAME} ${PERF_LIB})
	install(TARGETS ${BINARY_NAME}-perf DESTINATION bin COMPONENT ${BINARY_NAME}-perf)
	install(FILES ${CMAKE_SOURCE_DIR}/tools/perf.py DESTINATION "${CMAKE_INSTALL_LIBDIR}/${BINARY_NAME}" COMPONENT ${BINARY_NAME}-perf)
endif()

if(3DS)
	add_executable(${BINARY_NAME}.elf ${CMAKE_SOURCE_DIR}/src/platform/3ds/main.c)
	target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} m ${OS_LIB})
	add_custom_command(TARGET ${BINARY_NAME}.elf POST_BUILD COMMAND ${3DSXTOOL} ${BINARY_NAME}.elf ${BINARY_NAME}.3dsx)
endif()

if(PSP)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/platform/psp ${CMAKE_BINARY_DIR}/psp)
endif()

if(PSP2)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/platform/psp2 ${CMAKE_BINARY_DIR}/psp2)
endif()

# Packaging
set(CPACK_PACKAGE_VERSION ${VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${LIB_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${LIB_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${LIB_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/README.md)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "mGBA Game Boy Advance Emulator")
set(CPACK_PACKAGE_VENDOR "Jeffrey Pfau")
set(CPACK_PACKAGE_CONTACT "Jeffrey Pfau <jeffrey@endrift.com>")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")

SET(CPACK_DEB_COMPONENT_INSTALL ON)

set(CPACK_STRIP_FILES ${BINARY_NAME})

install(FILES ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/CHANGES DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT lib${BINARY_NAME})

include(CPack)

# Summaries
message(STATUS "Feature summary:")
message(STATUS "	CLI debugger: ${USE_CLI_DEBUGGER}")
message(STATUS "	GDB stub: ${USE_GDB_STUB}")
message(STATUS "	Video recording: ${USE_FFMPEG}")
message(STATUS "	GIF recording: ${USE_MAGICK}")
message(STATUS "	Screenshot/advanced savestate support: ${USE_PNG}")
message(STATUS "	ZIP support: ${USE_LIBZIP}")
message(STATUS "	7-Zip support: ${USE_LZMA}")
message(STATUS "	Better audio resampling: ${USE_BLIP}")
message(STATUS "Frontend summary:")
message(STATUS "	Qt: ${BUILD_QT}")
message(STATUS "	SDL (${SDL_VERSION}): ${BUILD_SDL}")
message(STATUS "	Libretro core: ${BUILD_LIBRETRO}")
message(STATUS "	Profiling: ${BUILD_PERF}")
message(STATUS "Library summary:")
message(STATUS "	Static: ${BUILD_STATIC}")
message(STATUS "	Shared: ${BUILD_SHARED}")
