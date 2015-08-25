file(GLOB PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/psp2/*.c)
include_directories(${CMAKE_BINARY_DIR})

execute_process(COMMAND ${RAW2C} ${CMAKE_SOURCE_DIR}/res/font.png)

set(PLATFORM_LIBRARY -lvita2d -lSceCtrl_stub -lSceRtc_stub -lSceGxm_stub -lSceDisplay_stub -lSceAudio_stub -lpng -lz -lm_stub)

add_executable(${BINARY_NAME}.elf ${PLATFORM_SRC} ${GUI_SRC} ${CMAKE_BINARY_DIR}/font.c)
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${PLATFORM_LIBRARY})
set_target_properties(${BINARY_NAME}.elf PROPERTIES OUTPUT_NAME ${BINARY_NAME}.elf)
add_custom_command(TARGET ${BINARY_NAME}.elf POST_BUILD COMMAND ${FIXUP} -q -S ${BINARY_NAME}.elf ${BINARY_NAME}.velf MAIN_DEPENDENCY ${BINARY_NAME}.elf)
