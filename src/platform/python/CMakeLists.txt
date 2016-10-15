find_program(PYTHON python)

set(PY_INCLUDE_DIRS -I${CMAKE_SOURCE_DIR}/src)
get_property(INCLUDE_DIRECTORIES DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
get_property(COMPILE_DEFINITIONS DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY COMPILE_DEFINITIONS)
foreach(INCLUDE_DIR IN LISTS INCLUDE_DIRECTORIES)
	list(APPEND PY_INCLUDE_DIRS -I${INCLUDE_DIR})
endforeach()
foreach(COMPILE_DEF IN LISTS COMPILE_DEFINITIONS FEATURE_DEFINES)
	list(APPEND PY_COMPILE_DEFS -D${COMPILE_DEF})
endforeach()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/setup.py)

add_custom_command(OUTPUT _builder.h
                   COMMAND ${CMAKE_C_COMPILER} ${PY_COMPILE_DEFS} ${PY_INCLUDE_DIRS} -fno-inline -E -P -c ${CMAKE_CURRENT_SOURCE_DIR}/_builder.h -o _builder.h
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/_builder.h)
add_custom_target(_builder.h ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/_builder.h)

add_custom_target(${BINARY_NAME}-pylib COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py ${PY_COMPILE_DEFS} ${PY_INCLUDE_DIRS}
                  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py
                  DEPENDS _builder.h)

add_custom_command(OUTPUT ${BINARY_NAME}/__init__.py
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${BINARY_NAME} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}
                   COMMAND ${PYTHON} ${CMAKE_CURRENT_BINARY_DIR}/setup.py build
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/setup.py
                   DEPENDS ${BINARY_NAME}-pylib)

add_custom_target(${BINARY_NAME}-py ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}/__init__.py)
