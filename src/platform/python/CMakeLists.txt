if(DEFINED ENV{VIRTUAL_ENV})
      find_program(PYTHON_EXECUTABLE NAMES python PATHS "$ENV{VIRTUAL_ENV}/bin")
endif()
find_package(PythonInterp ${USE_PYTHON_VERSION})
find_package(PythonLibs ${USE_PYTHON_VERSION})
list(APPEND DEPENDENCY_LIB ${PYTHON_LIBRARIES})
include_directories(AFTER ${PYTHON_INCLUDE_DIRS})

get_property(INCLUDE_DIRECTORIES DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
get_target_property(COMPILE_DEFINITIONS_BIN ${BINARY_NAME} COMPILE_DEFINITIONS)
get_directory_property(COMPILE_DEFINITIONS_DIR DIRECTORY . COMPILE_DEFINITIONS)
set(COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS_BIN} ${COMPILE_DEFINITIONS_DIR})
get_target_property(LIBRARIES ${BINARY_NAME} LINK_LIBRARIES)
set(LINK_LIBRARIES)
foreach(LIB IN LISTS LIBRARIES)
    if(TARGET ${LIB})
        list(APPEND LINK_LIBRARIES "$<TARGET_LINKER_FILE:${LIB}>")
    else()
        list(APPEND LINK_LIBRARIES ${LIB})
    endif()
endforeach()

set(CONFIG_IN "${CMAKE_CURRENT_SOURCE_DIR}/config.json.in")
set(CONFIG_OUT_I "${CMAKE_CURRENT_BINARY_DIR}/config.json.i")
set(CONFIG_OUT "${CMAKE_CURRENT_BINARY_DIR}/config.json")

configure_file("${CONFIG_IN}" "${CONFIG_OUT_I}")
file(GENERATE OUTPUT "${CONFIG_OUT}" INPUT "${CONFIG_OUT_I}")

file(GLOB PYTHON_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib.c
                   COMMAND ${CMAKE_COMMAND} -E env "JSON_CONFIG=${CONFIG_OUT}" ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py
                   COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/lib.c
                   DEPENDS ${PYTHON_HEADERS}
                   DEPENDS ${CONFIG_OUT}
                   DEPENDS ${BINARY_NAME}
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/lib.c PROPERTIES GENERATED ON)

file(GLOB PYTHON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
add_library(${BINARY_NAME}-pylib STATIC ${CMAKE_CURRENT_BINARY_DIR}/lib.c ${PYTHON_SRC})
set_target_properties(${BINARY_NAME}-pylib PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR};${INCLUDE_DIRECTORIES}")
set_target_properties(${BINARY_NAME}-pylib PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")

add_custom_target(${BINARY_NAME}-py ALL
                  COMMAND ${CMAKE_COMMAND} -E env "JSON_CONFIG=${CONFIG_OUT}" ${PYTHON_EXECUTABLE} setup.py build -b ${CMAKE_CURRENT_BINARY_DIR}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${BINARY_NAME}
                  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/setup.py
                  DEPENDS ${PYTHON_HEADERS}
                  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/_builder.py
                  DEPENDS ${BINARY_NAME}-pylib)

add_custom_target(${BINARY_NAME}-py-install
                  COMMAND ${CMAKE_COMMAND} -E env "JSON_CONFIG=${CONFIG_OUT}" ${PYTHON_EXECUTABLE} setup.py install -b ${CMAKE_CURRENT_BINARY_DIR}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${BINARY_NAME}-py)

add_custom_target(${BINARY_NAME}-py-develop
                  COMMAND ${CMAKE_COMMAND} -E env "JSON_CONFIG=${CONFIG_OUT}" ${PYTHON_EXECUTABLE} setup.py develop -b ${CMAKE_CURRENT_BINARY_DIR}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${BINARY_NAME}-py)

add_custom_target(${BINARY_NAME}-py-bdist
                  COMMAND ${CMAKE_COMMAND} -E env "JSON_CONFIG=${CONFIG_OUT}" ${PYTHON_EXECUTABLE} setup.py bdist_wheel -b ${CMAKE_CURRENT_BINARY_DIR}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${BINARY_NAME}-py)

file(GLOB TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*/test_*.py)
foreach(TEST IN LISTS TESTS)
	if(APPLE)
		set(PATH DYLD_LIBRARY_PATH)
	elseif(WIN32)
		set(PATH PATH)
	else()
		set(PATH LD_LIBRARY_PATH)
	endif()
	string(REGEX REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/tests/(.*/)?test_" "" TEST_NAME "${TEST}")
	string(REPLACE ".py" "" TEST_NAME "${TEST_NAME}")
	add_test(NAME python-${TEST_NAME}
             COMMAND ${CMAKE_COMMAND} -E env "JSON_CONFIG=${CONFIG_OUT}" "${PATH}=${CMAKE_CURRENT_BINARY_DIR}/.."
                     ${PYTHON_EXECUTABLE} setup.py build -b ${CMAKE_CURRENT_BINARY_DIR} pytest --extras --addopts ${TEST}
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
