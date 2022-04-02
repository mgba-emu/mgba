find_program(ELF2NRO elf2nro)
find_program(NACPTOOL nacptool)
find_program(BUILD_ROMFS build_romfs)
find_library(GLAPI_LIBRARY glapi REQUIRED)
find_library(EGL_LIBRARY EGL REQUIRED)

set(OS_DEFINES _GNU_SOURCE IOAPI_NO_64)
list(APPEND CORE_VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-fd.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c)
list(APPEND GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c)

include_directories(AFTER ${OPENGLES3_INCLUDE_DIR} ${OPENGL_EGL_INCLUDE_DIR})

if(${CMAKE_BUILD_TYPE} STREQUAL Debug OR ${CMAKE_BUILD_TYPE} STREQUAL RelWithDebInfo)
	find_library(NOUVEAU_LIBRARY drm_nouveaud REQUIRED)
	list(APPEND OS_LIB nxd)
else()
	find_library(NOUVEAU_LIBRARY drm_nouveau REQUIRED)
	list(APPEND OS_LIB nx)
endif()
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)

if(BUILD_PERF)
	add_custom_target(${BINARY_NAME}-perf.nro ALL
	                  ${ELF2NRO} ../${BINARY_NAME}-perf ${BINARY_NAME}-perf.nro
	                  DEPENDS ${BINARY_NAME}-perf)
	install(FILES
		    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}-perf.nro
		    DESTINATION . COMPONENT ${BINARY_NAME}-perf)
endif()

add_executable(${BINARY_NAME}.elf ${GUI_SRC} ${PLATFORM_SRC} main.c)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${M_LIBRARY} ${EGL_LIBRARY} ${OPENGLES3_LIBRARY} ${GLAPI_LIBRARY} ${NOUVEAU_LIBRARY} stdc++ ${OS_LIB})

add_custom_command(OUTPUT control.nacp
                   COMMAND ${NACPTOOL} --create "${PROJECT_NAME}" "endrift" "${VERSION_STRING}" control.nacp)

add_custom_command(OUTPUT romfs.bin
                   COMMAND ${CMAKE_COMMAND} -E make_directory romfs
                   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/res/font-new.png" romfs/
                   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/fileassoc.cfg.in" romfs/
                   COMMAND ${BUILD_ROMFS} romfs romfs.bin
                   COMMAND ${CMAKE_COMMAND} -E remove_directory romfs
               DEPENDS "${CMAKE_SOURCE_DIR}/res/font-new.png" "${CMAKE_CURRENT_SOURCE_DIR}/fileassoc.cfg.in")

add_custom_target(${BINARY_NAME}.nro ALL
                  ${ELF2NRO} ${BINARY_NAME}.elf ${BINARY_NAME}.nro --romfs=romfs.bin --nacp=control.nacp --icon="${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg"
                  DEPENDS ${BINARY_NAME}.elf control.nacp ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg romfs.bin)

install(TARGETS ${BINARY_NAME}.elf DESTINATION . COMPONENT ${BINARY_NAME}-dbg)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.nro DESTINATION . COMPONENT ${BINARY_NAME}-switch)
