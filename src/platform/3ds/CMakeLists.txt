set(USE_VFS_3DS OFF CACHE BOOL "Use 3DS-specific file support")
mark_as_advanced(USE_VFS_3DS)

find_program(3DSLINK 3dslink)
find_program(3DSXTOOL 3dsxtool)
find_program(BANNERTOOL bannertool)
find_program(MAKEROM makerom)
find_program(PICASSO picasso)
find_program(RAW2C raw2c)
find_program(TEX3DS tex3ds)
set(STRIP "${cross_prefix_path}strip" CACHE INTERNAL "symbol stripper")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format" PARENT_SCOPE)
set(OS_DEFINES _GNU_SOURCE COLOR_16_BIT COLOR_5_6_5 FIXED_ROM_BUFFER IOAPI_NO_64)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
if(${CMAKE_BUILD_TYPE} STREQUAL Debug OR ${CMAKE_BUILD_TYPE} STREQUAL RelWithDebInfo)
	list(APPEND OS_LIB citro3dd ctrud)
else()
	list(APPEND OS_LIB citro3d ctru)
endif()
file(GLOB OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/3ds-*.c ${CMAKE_CURRENT_SOURCE_DIR}/ctru-heap.c ${CMAKE_CURRENT_SOURCE_DIR}/socket.c)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)
source_group("3DS-specific code" FILES ${OS_SRC})

if(USE_VFS_3DS)
	list(APPEND OS_DEFINES USE_VFS_3DS)
else()
	list(APPEND CORE_VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-fd.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c)
endif()
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

list(APPEND GUI_SRC
	${CMAKE_CURRENT_BINARY_DIR}/icons.c
	${CMAKE_CURRENT_BINARY_DIR}/uishader.c
	${CMAKE_CURRENT_BINARY_DIR}/uishader.h
	${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin.h

	${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c
	${CMAKE_CURRENT_SOURCE_DIR}/ctr-gpu.c
	${CMAKE_CURRENT_SOURCE_DIR}/ctr-gpu.h)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/icons.c
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

add_custom_command(OUTPUT ${BINARY_NAME}.xml
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/hbl.xml ${BINARY_NAME}.xml
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hbl.xml)

add_custom_command(OUTPUT ${BINARY_NAME}.bnr
                   COMMAND ${BANNERTOOL} makebanner -ci ${CMAKE_CURRENT_SOURCE_DIR}/banner.cgfx -a ${CMAKE_CURRENT_SOURCE_DIR}/bios.wav -o ${BINARY_NAME}.bnr
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/banner.cgfx ${CMAKE_CURRENT_SOURCE_DIR}/bios.wav)

# tex3ds binaries as of 2.0.1-3 crash if you try to do this
#add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.t3x
#                   COMMAND ${TEX3DS} -f rgb5551 -o icons.t3x ${CMAKE_SOURCE_DIR}/res/icons.png
#                   MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/res/icons.png
#                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.c
                   COMMAND ${RAW2C} ${CMAKE_CURRENT_SOURCE_DIR}/icons.t3x
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icons.t3x)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/uishader.v.pica ${CMAKE_CURRENT_SOURCE_DIR}/uishader.g.pica
	COMMAND ${PICASSO}
		-o ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin
		-h ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin.h
		${CMAKE_CURRENT_SOURCE_DIR}/uishader.v.pica
		${CMAKE_CURRENT_SOURCE_DIR}/uishader.g.pica
	COMMENT "picasso uishader.shbin")

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uishader.c ${CMAKE_CURRENT_BINARY_DIR}/uishader.h
	MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin
	COMMAND ${RAW2C} ${CMAKE_CURRENT_BINARY_DIR}/uishader.shbin
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "raw2c uishader.shbin")

add_custom_command(OUTPUT ${BINARY_NAME}.3dsx
                   COMMAND ${3DSXTOOL} ${BINARY_NAME}.elf ${BINARY_NAME}.3dsx --smdh=${BINARY_NAME}.smdh
                   DEPENDS ${BINARY_NAME}.elf ${BINARY_NAME}.smdh ${BINARY_NAME}.xml)
add_custom_target(${BINARY_NAME}.3dsx ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx)

add_custom_command(OUTPUT ${BINARY_NAME}.cia
                   COMMAND ${STRIP} -o ${BINARY_NAME}-stripped.elf ${BINARY_NAME}.elf
                   COMMAND ${MAKEROM} -f cia -o ${BINARY_NAME}.cia -rsf cia.rsf -target t -exefslogo -elf ${BINARY_NAME}-stripped.elf -icon ${BINARY_NAME}.smdh -banner ${BINARY_NAME}.bnr -major ${LIB_VERSION_MAJOR} -minor ${LIB_VERSION_MINOR} -micro ${LIB_VERSION_PATCH}
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf ${BINARY_NAME}.elf ${BINARY_NAME}.smdh ${BINARY_NAME}.bnr)
add_custom_target(${BINARY_NAME}.cia ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia)

add_custom_target(run ${3DSLINK} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
                  DEPENDS ${BINARY_NAME}.3dsx)

add_custom_target(install-cia stat -f '0 00000001 %016Xz' '${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia' | xxd -r | cat -- - '${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia' | nc $ENV{N3DS_IP} 5000
                  DEPENDS ${BINARY_NAME}.cia)

if(BUILD_PERF)
	add_custom_target(${BINARY_NAME}-perf.3dsx ALL
	                  ${3DSXTOOL} ../${BINARY_NAME}-perf ${BINARY_NAME}-perf.3dsx --smdh=${BINARY_NAME}.smdh
	                  DEPENDS ${BINARY_NAME}-perf ${BINARY_NAME}.smdh)
	install(FILES
		    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}-perf.3dsx
		    DESTINATION . COMPONENT ${BINARY_NAME}-perf)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cia.rsf.in ${CMAKE_CURRENT_BINARY_DIR}/cia.rsf)

install(TARGETS ${BINARY_NAME}.elf DESTINATION . COMPONENT ${BINARY_NAME}-dbg)
install(FILES
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.3dsx
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.smdh
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.xml
	    DESTINATION 3dsx COMPONENT ${BINARY_NAME}-3ds)
install(FILES
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia
	    DESTINATION cia COMPONENT ${BINARY_NAME}-3ds)
