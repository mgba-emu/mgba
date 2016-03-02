set(USE_VFS_3DS ON CACNE BOOL "Use 3DS-specific file support")
mark_as_advanced(USE_VFS_3DS)

find_program(3DSLINK 3dslink)
find_program(3DSXTOOL 3dsxtool)
find_program(BANNERTOOL bannertool)
find_program(MAKEROM makerom)
find_program(PICASSO picasso)
find_program(RAW2C raw2c)
find_program(STRIP ${cross_prefix}strip)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format" PARENT_SCOPE)
set(OS_DEFINES COLOR_16_BIT COLOR_5_6_5 IOAPI_NO_64)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
list(APPEND OS_LIB ctru)
file(GLOB OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/3ds-*.c ${CMAKE_CURRENT_SOURCE_DIR}/ctru-heap.c ${CMAKE_CURRENT_SOURCE_DIR}/socket.c)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)
source_group("3DS-specific code" FILES ${OS_SRC})

if(USE_VFS_3DS)
	list(APPEND OS_DEFINES USE_VFS_3DS)
else()
	list(APPEND OS_DEFINES USE_VFS_FILE)
	list(APPEND CORE_VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-file.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c)
endif()
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

list(APPEND GUI_SRC
	${CMAKE_CURRENT_BINARY_DIR}/icons.c
	${CMAKE_CURRENT_BINARY_DIR}/font.c
	${CMAKE_CURRENT_BINARY_DIR}/uishader.c
	${CMAKE_CURRENT_BINARY_DIR}/uishader.h
	${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin.h

	${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c
	${CMAKE_CURRENT_SOURCE_DIR}/ctr-gpu.c
	${CMAKE_CURRENT_SOURCE_DIR}/ctr-gpu.h)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/icons.c
	${CMAKE_CURRENT_BINARY_DIR}/font.c
	${CMAKE_CURRENT_BINARY_DIR}/uishader.c
	${CMAKE_CURRENT_BINARY_DIR}/uishader.h
	${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin.h
	PROPERTIES GENERATED ON)
add_executable(${BINARY_NAME}.elf ${GUI_SRC} main.c)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
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

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.c
                   COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/src/platform/3ds/icons.raw
                   DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/3ds/icons.raw)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin.h
	MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/uishader.vsh
	COMMAND ${PICASSO}
		-o ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin
		-h ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin.h
		${CMAKE_CURRENT_SOURCE_DIR}/uishader.vsh
	COMMENT "picasso uishader.vsh")

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uishader.c ${CMAKE_CURRENT_BINARY_DIR}/uishader.h
	MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin
	COMMAND ${RAW2C} ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "raw2c uishader.shbin")

add_custom_target(${BINARY_NAME}.3dsx ALL
                  ${3DSXTOOL} ${BINARY_NAME}.elf ${BINARY_NAME}.3dsx --smdh=${BINARY_NAME}.smdh
                  DEPENDS ${BINARY_NAME}.elf ${BINARY_NAME}.smdh)

add_custom_target(${BINARY_NAME}.cia ALL
                  ${STRIP} -o ${BINARY_NAME}-stripped.elf ${BINARY_NAME}.elf
                  COMMAND ${MAKEROM} -f cia -o ${BINARY_NAME}.cia -rsf cia.rsf -target t -exefslogo -elf ${BINARY_NAME}-stripped.elf -icon ${BINARY_NAME}.smdh -banner ${BINARY_NAME}.bnr
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf ${BINARY_NAME}.elf ${BINARY_NAME}.smdh ${BINARY_NAME}.bnr)

add_custom_target(run ${3DSLINK} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
                  DEPENDS ${BINARY_NAME}.3dsx)

if(BUILD_PERF)
	add_custom_target(${BINARY_NAME}-perf.3dsx ALL
	                  ${3DSXTOOL} ../${BINARY_NAME}-perf ${BINARY_NAME}-perf.3dsx --smdh=${BINARY_NAME}.smdh
	                  DEPENDS ${BINARY_NAME}-perf ${BINARY_NAME}.smdh)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cia.rsf.in ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf)
install(FILES
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.smdh
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia
	    DESTINATION . COMPONENT ${BINARY_NAME}-3ds)
