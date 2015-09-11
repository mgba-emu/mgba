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
                   COMMAND ${BANNERTOOL} makesmdh -s "${PROJECT_NAME}" -l "${SUMMARY}" -p "endrift" -i ${CMAKE_SOURCE_DIR}/res/mgba-48.png -o ${BINARY_NAME}.smdh
                   DEPENDS ${CMAKE_SOURCE_DIR}/res/mgba-48.png)

add_custom_command(OUTPUT ${BINARY_NAME}.bnr
                   COMMAND ${BANNERTOOL} makebanner -i ${CMAKE_CURRENT_SOURCE_DIR}/logo.png -a ${CMAKE_CURRENT_SOURCE_DIR}/bios.wav -o ${BINARY_NAME}.bnr
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/logo.png ${CMAKE_CURRENT_SOURCE_DIR}/bios.wav)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.c
                   COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/src/platform/3ds/font.raw
                   DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/3ds/font.raw)

add_custom_target(${BINARY_NAME}.3dsx ALL
                  ${3DSXTOOL} ${BINARY_NAME}.elf ${BINARY_NAME}.3dsx --smdh=${BINARY_NAME}.smdh
                  DEPENDS ${BINARY_NAME}.elf ${BINARY_NAME}.smdh)

add_custom_target(${BINARY_NAME}.cia ALL
                  ${STRIP} -o ${BINARY_NAME}-stripped.elf ${BINARY_NAME}.elf
                  COMMAND ${MAKEROM} -f cia -o ${BINARY_NAME}.cia -rsf cia.rsf -target t -exefslogo -elf ${BINARY_NAME}-stripped.elf -icon ${BINARY_NAME}.smdh -banner ${BINARY_NAME}.bnr
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf ${BINARY_NAME}.elf ${BINARY_NAME}.smdh ${BINARY_NAME}.bnr)

add_custom_target(run ${3DSLINK} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
                  DEPENDS ${BINARY_NAME}.3dsx)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cia.rsf.in ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf)
install(FILES
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.smdh
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia
	    DESTINATION . COMPONENT ${BINARY_NAME}-3ds)
