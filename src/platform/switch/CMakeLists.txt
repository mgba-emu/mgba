find_program(ELF2NRO elf2nro)
find_program(NXLINK, nxlink)
find_program(RAW2C raw2c)
find_program(STRIP ${cross_prefix}strip)

set(OS_DEFINES USE_VFS_FILE IOAPI_NO_64)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format" PARENT_SCOPE)
list(APPEND CORE_VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-file.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-devlist.c)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
file(GLOB OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/nx-heap.c ${CMAKE_CURRENT_SOURCE_DIR}/switch-memory.c)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)
source_group("Switch-specific code" FILES ${OS_SRC})

set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

list(APPEND GUI_SRC
	${CMAKE_CURRENT_BINARY_DIR}/icons.c
	${CMAKE_CURRENT_BINARY_DIR}/font.c

	${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c
	${CMAKE_CURRENT_SOURCE_DIR}/nx-gfx.c
	${CMAKE_CURRENT_SOURCE_DIR}/nx-gfx.h)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/icons.c
	${CMAKE_CURRENT_BINARY_DIR}/font.c
	PROPERTIES GENERATED ON)

add_executable(${BINARY_NAME}.elf ${GUI_SRC} main.c)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${M_LIBRARY} ${OS_LIB} nx)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.c
                   COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/src/platform/switch/icons.raw
				   DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/switch/icons.raw)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.c
                   COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/src/platform/switch/font.raw
				   DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/switch/font.raw)

add_custom_command(OUTPUT ${BINARY_NAME}.nro
                   COMMAND ${ELF2NRO} ${BINARY_NAME}.elf ${BINARY_NAME}.nro --icon=${CMAKE_SOURCE_DIR}/src/platform/switch/app_icon.jpg
                   DEPENDS ${BINARY_NAME}.elf)
add_custom_target(${BINARY_NAME}.nro ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.nro)

add_custom_target(run ${NXLINK} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.nro
                  DEPENDS ${BINARY_NAME}.nro)

if(BUILD_PERF)
	add_custom_target(${BINARY_NAME}-perf.nro ALL
	                  ${ELF2NRO} ../${BINARY_NAME}-perf ${BINARY_NAME}-perf.nro --icon=${CMAKE_SOURCE_DIR}/src/platform/switch/app_icon.jpg
	                  DEPENDS ${BINARY_NAME}-perf)
	install(FILES
		    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}-perf.nro
		    DESTINATION . COMPONENT ${BINARY_NAME}-perf)
endif()

install(FILES
	    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.nro
	    DESTINATION nro COMPONENT ${BINARY_NAME}-nro)
