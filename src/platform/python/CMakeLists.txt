find_program(PYTHON python)

get_property(INCLUDE_DIRECTORIES DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
set(INCLUDE_FLAGS)
foreach(DIR IN LISTS INCLUDE_DIRECTORIES)
	list(APPEND INCLUDE_FLAGS "-I${DIR}")
endforeach()

include(FindPythonLibs)
list(APPEND DEPENDENCY_LIB ${PYTHON_LIBRARIES})
include_directories(AFTER ${PYTHON_INCLUDE_DIRS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
add_custom_command(OUTPUT build/lib/${BINARY_NAME}/__init__.py
                   COMMAND BINDIR=${CMAKE_CURRENT_BINARY_DIR}/.. CPPFLAGS="${INCLUDE_FLAGS}" ${PYTHON} ${CMAKE_CURRENT_BINARY_DIR}/setup.py build --build-base ${CMAKE_CURRENT_BINARY_DIR}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ${BINARY_NAME}
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/setup.py
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py)

add_custom_command(OUTPUT lib.c
                  COMMAND BINDIR=${CMAKE_CURRENT_BINARY_DIR}/.. CPPFLAGS="${INCLUDE_FLAGS}" ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py
                  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lib.h
                  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/lib.c PROPERTIES GENERATED ON)

file(GLOB PYTHON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
add_library(${BINARY_NAME}-pylib STATIC ${CMAKE_CURRENT_BINARY_DIR}/lib.c ${PYTHON_SRC})
add_dependencies(${BINARY_NAME}-pylib ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py)
set_target_properties(${BINARY_NAME}-pylib PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR};${INCLUDE_DIRECTORIES}")
set(PYTHON_LIBRARY ${BINARY_NAME}-pylib PARENT_SCOPE)

add_custom_target(${BINARY_NAME}-py ALL DEPENDS ${BINARY_NAME}-pylib ${CMAKE_CURRENT_BINARY_DIR}/build/lib/${BINARY_NAME}/__init__.py)
