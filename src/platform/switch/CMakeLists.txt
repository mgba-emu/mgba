find_program(ELF2NRO elf2nro)

set(OS_DEFINES USE_VFS_FILE IOAPI_NO_64)
list(APPEND CORE_VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-file.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c)

file(GLOB OS_SRC ${CMAKE_SOURCE_DIR}/src/platform/wii/wii-*.c)
if(${CMAKE_BUILD_TYPE} STREQUAL Debug OR ${CMAKE_BUILD_TYPE} STREQUAL RelWithDebInfo)
	list(APPEND OS_LIB nxd)
else()
	list(APPEND OS_LIB nx)
endif()
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)

if(BUILD_PERF)
	add_custom_target(${BINARY_NAME}-perf.nro ALL
	                  ${ELF2NRO} ../${BINARY_NAME}-perf ${BINARY_NAME}-perf.nro
	                  DEPENDS ${BINARY_NAME}-perf)
	install(FILES
		    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}-perf.nro
		    DESTINATION . COMPONENT ${BINARY_NAME}-perf)
endif()
