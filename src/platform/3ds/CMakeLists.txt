list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(Tools3DS)

set(USE_VFS_3DS ON CACNE BOOL "Use 3DS-specific file support")
mark_as_advanced(USE_VFS_3DS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format" PARENT_SCOPE)
set(OS_DEFINES COLOR_16_BIT COLOR_5_6_5)

find_package(CTRULIB REQUIRED)
find_package(SF2D REQUIRED)
list(APPEND OS_LIB 3ds::sf2d 3ds::ctrulib)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
file(GLOB OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/3ds-*.c)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
source_group("3DS-specific code" FILES ${OS_SRC})

if(USE_VFS_3DS)
	list(APPEND OS_DEFINES USE_VFS_3DS)
else()
	list(APPEND OS_DEFINES USE_VFS_FILE)
	list(APPEND VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-file.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c)
endif()
set(VFS_SRC ${VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

list(APPEND GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/font.c PROPERTIES GENERATED ON)
add_executable(${BINARY_NAME}.elf ${GUI_SRC} main.c ctru-heap.c)
target_compile_definitions(${BINARY_NAME}.elf PUBLIC "${OS_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${M_LIBRARY} ${OS_LIB})
target_embed_file(${BINARY_NAME}.elf ${CMAKE_CURRENT_LIST_DIR}/font.raw) 

set(APP_TITLE "${PROJECT_NAME}")
set(APP_DESCRIPTION "${SUMMARY}")
set(APP_AUTHOR "endrift")
set(APP_ICON ${CMAKE_SOURCE_DIR}/res/mgba-48.png)

add_3dsx_target(${BINARY_NAME}.elf)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cia.rsf.in ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf)
add_cia_target(${BINARY_NAME}.elf ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf ${CMAKE_CURRENT_SOURCE_DIR}/logo.png ${CMAKE_CURRENT_SOURCE_DIR}/bios.wav)

add_netload_target(run ${BINARY_NAME}.3dsx)

install(FILES
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.smdh
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia
	    DESTINATION . COMPONENT ${BINARY_NAME}-3ds)
