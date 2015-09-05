file(GLOB OS_SRC ${CMAKE_SOURCE_DIR}/src/platform/psp2/psp2-*.c)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
source_group("PS Vita-specific code" FILES ${OS_SRC})

list(APPEND VFS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sce-vfs.c)
set(VFS_SRC ${VFS_SRC} PARENT_SCOPE)

set(OS_LIB -lvita2d -lSceCtrl_stub -lSceRtc_stub -lSceGxm_stub -lSceDisplay_stub -lSceAudio_stub -lSceMotion_stub -lScePower_stub -lSceTouch_stub -lpng -lz -l${M_LIBRARY})
set(OBJCOPY_CMD ${OBJCOPY} -I binary -O elf32-littlearm -B arm)

list(APPEND GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/font.c ${CMAKE_CURRENT_BINARY_DIR}/backdrop.c PROPERTIES GENERATED ON)
add_executable(${BINARY_NAME}.elf ${PLATFORM_SRC} ${GUI_SRC} ${CMAKE_BINARY_DIR}/font.o ${CMAKE_BINARY_DIR}/backdrop.o main.c)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${OS_LIB})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.c
                   COMMAND ${OBJCOPY_CMD} font.png ${CMAKE_CURRENT_BINARY_DIR}/font.o
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/res)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/backdrop.c
                   COMMAND ${OBJCOPY_CMD} backdrop.png ${CMAKE_CURRENT_BINARY_DIR}/backdrop.o
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/res)

add_custom_target(${BINARY_NAME}.velf ALL
                  ${FIXUP} ${BINARY_NAME}.elf ${BINARY_NAME}.velf ${NIDDB}
                  DEPENDS ${BINARY_NAME}.elf)

install(FILES ${BINARY_NAME}.velf DESTINATION . COMPONENT ${BINARY_NAME}-psp2)
