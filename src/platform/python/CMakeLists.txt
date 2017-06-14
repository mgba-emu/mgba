find_program(PYTHON python)

get_property(INCLUDE_DIRECTORIES DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
set(INCLUDE_FLAGS)
foreach(DIR IN LISTS INCLUDE_DIRECTORIES)
	list(APPEND INCLUDE_FLAGS "-I${DIR}")
endforeach()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
add_custom_command(OUTPUT build/lib/${BINARY_NAME}/__init__.py
                   COMMAND BINDIR=${CMAKE_CURRENT_BINARY_DIR}/.. CPPFLAGS="${INCLUDE_FLAGS}" ${PYTHON} ${CMAKE_CURRENT_BINARY_DIR}/setup.py build --build-base ${CMAKE_CURRENT_BINARY_DIR}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ${BINARY_NAME}
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/setup.py
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py)

add_custom_target(${BINARY_NAME}-py ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build/lib/${BINARY_NAME}/__init__.py)
