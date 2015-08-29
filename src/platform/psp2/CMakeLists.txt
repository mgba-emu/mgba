file(GLOB PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/psp2/*.c)

execute_process(COMMAND ${OBJCOPY} -I binary -O elf32-littlearm -B arm font.png ${CMAKE_BINARY_DIR}/font.o WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/res)

set(PLATFORM_LIBRARY -lvita2d -lSceCtrl_stub -lSceRtc_stub -lSceGxm_stub -lSceDisplay_stub -lSceAudio_stub -lpng -lz -l${M_LIBRARY})

add_executable(${BINARY_NAME}.elf ${PLATFORM_SRC} ${GUI_SRC} ${CMAKE_BINARY_DIR}/font.o)
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${PLATFORM_LIBRARY})
set_target_properties(${BINARY_NAME}.elf PROPERTIES OUTPUT_NAME ${BINARY_NAME}.elf)
add_custom_command(TARGET ${BINARY_NAME}.elf POST_BUILD COMMAND ${FIXUP} ${BINARY_NAME}.elf ${BINARY_NAME}.velf ${NIDDB} MAIN_DEPENDENCY ${BINARY_NAME}.elf)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.velf DESTINATION . COMPONENT ${BINARY_NAME}-psp2)
