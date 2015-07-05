file(GLOB PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/psp2/*.c)

set(PLATFORM_LIBRARY -lvita2d -lSceCtrl_stub -lSceRtc_stub -lSceGxm_stub -lSceDisplay_stub -lm_stub)

add_executable(${BINARY_NAME}.elf ${PLATFORM_SRC})
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${PLATFORM_LIBRARY})
set_target_properties(${BINARY_NAME}.elf PROPERTIES OUTPUT_NAME ${BINARY_NAME}.elf)
add_custom_command(TARGET ${BINARY_NAME}.elf POST_BUILD COMMAND ${FIXUP} -q -S ${BINARY_NAME}.elf ${BINARY_NAME}.velf MAIN_DEPENDENCY ${BINARY_NAME}.elf)
