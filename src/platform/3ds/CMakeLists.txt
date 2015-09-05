set(USE_VFS_3DS ON CACNE BOOL "Use 3DS-specific file support")
mark_as_advanced(USE_VFS_3DS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format" PARENT_SCOPE)
set(OS_DEFINES COLOR_16_BIT COLOR_5_6_5)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
list(APPEND OS_LIB sf2d ctru)
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

list(APPEND GUI_SRC ${CMAKE_CURRENT_BINARY_DIR}/font.c ${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/font.c PROPERTIES GENERATED ON)
add_executable(${BINARY_NAME}.elf ${GUI_SRC} main.c ctru-heap.c)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${M_LIBRARY} ${OS_LIB})

add_custom_command(OUTPUT ${BINARY_NAME}.smdh
                   COMMAND ${SMDHTOOL} --create "${PROJECT_NAME}" "${SUMMARY}" "endrift" ${CMAKE_SOURCE_DIR}/res/mgba-48.png ${BINARY_NAME}.smdh)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.c
                   COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/src/platform/3ds/font.raw)

add_custom_target(${BINARY_NAME}.3dsx ALL
	              ${3DSXTOOL} ${BINARY_NAME}.elf ${BINARY_NAME}.3dsx --smdh=${BINARY_NAME}.smdh
	              DEPENDS ${BINARY_NAME}.elf ${BINARY_NAME}.smdh)

add_custom_target(run ${3DSLINK} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
                  DEPENDS ${BINARY_NAME}.3dsx)

install(FILES ${BINARY_NAME}.3dsx ${BINARY_NAME}.smdh DESTINATION . COMPONENT ${BINARY_NAME}-3ds)
