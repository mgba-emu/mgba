if(NOT ${BINARY_NAME}_SOURCE_DIR)
	set(${BINARY_NAME}_SOURCE_DIR ${CMAKE_SOURCE_DIR})
endif()

configure_file("${${BINARY_NAME}_SOURCE_DIR}/src/util/version.c.in" "${CMAKE_CURRENT_BINARY_DIR}/version.c")
