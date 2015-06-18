file(GLOB PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/psp/*.c)

set(PLATFORM_LIBRARY c pspctrl pspdisplay pspge psppower pspuser)

add_executable(${BINARY_NAME}.elf ${PLATFORM_SRC})
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${PLATFORM_LIBRARY})
set_target_properties(${BINARY_NAME}.elf PROPERTIES OUTPUT_NAME ${BINARY_NAME}.elf)
add_custom_command(TARGET ${BINARY_NAME}.elf POST_BUILD COMMAND ${FIXUP} ${BINARY_NAME}.elf MAIN_DEPENDENCY ${BINARY_NAME}.elf)
add_custom_command(OUTPUT PARAM.SFO COMMAND ${MKSFO} ${PROJECT_NAME} PARAM.SFO MAIN_DEPENDENCY ${BINARY_NAME}.elf)
add_custom_target(${BINARY_NAME}.prx ${PRXGEN} ${BINARY_NAME}.elf ${BINARY_NAME}.prx DEPENDS ${BINARY_NAME}.elf)
add_custom_target(${BINARY_NAME}.pbp ALL ${PACK_PBP} EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL ${BINARY_NAME}.prx NULL DEPENDS ${BINARY_NAME}.prx PARAM.SFO)
