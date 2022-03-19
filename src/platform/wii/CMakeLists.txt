find_program(ELF2DOL elf2dol)
find_program(GXTEXCONV gxtexconv)
find_program(RAW2C raw2c)
find_program(WIILOAD wiiload)

find_library(WIIDRC_LIBRARY wiidrc)
if(WIIDRC_LIBRARY)
    add_definitions(-DWIIDRC)
endif()

set(OS_DEFINES _GNU_SOURCE COLOR_16_BIT COLOR_5_6_5 IOAPI_NO_64 FIXED_ROM_BUFFER)
list(APPEND CORE_VFS_SRC ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-fd.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c ${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-devlist.c)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

list(APPEND OS_LIB wiiuse bte fat ogc)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)
source_group("Wii-specific code" FILES ${OS_SRC})
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

list(APPEND GUI_SRC ${CMAKE_CURRENT_BINARY_DIR}/font.c ${CMAKE_CURRENT_BINARY_DIR}/icons.c ${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/font.c PROPERTIES GENERATED ON)
add_executable(${BINARY_NAME}.elf ${GUI_SRC} main.c)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${OS_LIB})
if(WIIDRC_LIBRARY)
    target_link_libraries(${BINARY_NAME}.elf ${WIIDRC_LIBRARY})
endif()

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.tpl
                   COMMAND ${GXTEXCONV} -i ${CMAKE_SOURCE_DIR}/res/font2x.png -o font.tpl colfmt=5 mipmap=no
                   MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/res/font2x.png
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/font.c
                   COMMAND ${RAW2C} ${CMAKE_CURRENT_BINARY_DIR}/font.tpl
                   MAIN_DEPENDENCY  ${CMAKE_CURRENT_BINARY_DIR}/font.tpl
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.tpl
                   COMMAND ${GXTEXCONV} -i ${CMAKE_SOURCE_DIR}/res/icons2x.png -o icons.tpl colfmt=5 mipmap=no
                   MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/res/icons2x.png
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.c
                   COMMAND ${RAW2C} ${CMAKE_CURRENT_BINARY_DIR}/icons.tpl
                   MAIN_DEPENDENCY  ${CMAKE_CURRENT_BINARY_DIR}/icons.tpl
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(${BINARY_NAME}.dol ALL
                  ${ELF2DOL} ${BINARY_NAME}.elf ${BINARY_NAME}.dol
                  DEPENDS ${BINARY_NAME}.elf)

add_custom_target(run ${WIILOAD} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.dol
                  DEPENDS ${BINARY_NAME}.dol)

if(BUILD_PERF)
	add_custom_target(${BINARY_NAME}-perf.dol ALL
	                  ${ELF2DOL} ../${BINARY_NAME}-perf ${BINARY_NAME}-perf.dol
	                  DEPENDS ${BINARY_NAME}-perf)

	install(FILES
		    ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}-perf.dol
		    DESTINATION . COMPONENT ${BINARY_NAME}-perf)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta.xml.in ${CMAKE_CURRENT_BINARY_DIR}/meta.xml)

install(TARGETS ${BINARY_NAME}.elf DESTINATION . COMPONENT ${BINARY_NAME}-dbg)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icon.png ${CMAKE_CURRENT_BINARY_DIR}/meta.xml DESTINATION . COMPONENT ${BINARY_NAME}-wii)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.dol DESTINATION . RENAME boot.dol COMPONENT ${BINARY_NAME}-wii)
