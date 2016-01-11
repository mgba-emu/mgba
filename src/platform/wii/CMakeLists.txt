find_program(ELF2DOL elf2dol)
find_program(GXTEXCONV gxtexconv)
find_program(RAW2C raw2c)
find_program(WIILOAD wiiload)

set(OS_DEFINES COLOR_16_BIT COLOR_5_6_5 USE_VFS_FILE IOAPI_NO_64)
list(APPEND CORE_VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-file.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-devlist.c)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB OS_SRC ${CMAKE_SOURCE_DIR}/src/platform/wii/wii-*.c)
list(APPEND OS_LIB wiiuse bte fat ogc)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
source_group("Wii-specific code" FILES ${OS_SRC})
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

list(APPEND GUI_SRC ${CMAKE_CURRENT_BINARY_DIR}/font.c ${CMAKE_CURRENT_BINARY_DIR}/icons.c ${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/font.c PROPERTIES GENERATED ON)
add_executable(${BINARY_NAME}.elf ${GUI_SRC} main.c)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${OS_LIB})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.c
                   COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/src/platform/wii/font.tpl
                   MAIN_DEPENDENCY  ${CMAKE_SOURCE_DIR}/src/platform/wii/font.tpl
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.c
                   COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/src/platform/wii/icons.tpl
                   MAIN_DEPENDENCY  ${CMAKE_SOURCE_DIR}/src/platform/wii/icons.tpl
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(${BINARY_NAME}.dol ALL
                  ${ELF2DOL} ${BINARY_NAME}.elf ${BINARY_NAME}.dol
                  DEPENDS ${BINARY_NAME}.elf)

add_custom_target(run ${WIILOAD} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.dol
                  DEPENDS ${BINARY_NAME}.dol)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta.xml.in ${CMAKE_CURRENT_BINARY_DIR}/meta.xml)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icon.png ${CMAKE_CURRENT_BINARY_DIR}/meta.xml DESTINATION . COMPONENT ${BINARY_NAME}-wii)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.dol DESTINATION . RENAME boot.dol COMPONENT ${BINARY_NAME}-wii)
