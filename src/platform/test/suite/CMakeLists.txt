file(GLOB SUITE_COMMON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/*.c)
add_executable(${BINARY_NAME}-suite ${SUITE_COMMON_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/../suite-main.c)
target_link_libraries(${BINARY_NAME}-suite ${BINARY_NAME} ${PLATFORM_LIBRARY} cmocka)
add_test(${BINARY_NAME}-suite ${BINARY_NAME}-suite)