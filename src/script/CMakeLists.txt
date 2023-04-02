include(ExportDirectory)
set(SOURCE_FILES
	context.c
	input.c
	image.c
	socket.c
	stdlib.c
	types.c)

set(TEST_FILES
	test/classes.c
	test/types.c)

if(USE_JSON_C)
	list(APPEND SOURCE_FILES storage.c)
endif()

if(USE_LUA)
	list(APPEND SOURCE_FILES engines/lua.c)
	list(APPEND TEST_FILES
		test/context.c
		test/image.c
		test/input.c
		test/lua.c
		test/stdlib.c)

	if(USE_JSON_C)
		list(APPEND TEST_FILES test/storage.c)
	endif()
endif()

source_group("Scripting" FILES ${SOURCE_FILES})
source_group("Scripting tests" FILES ${TEST_FILES})

export_directory(SCRIPT SOURCE_FILES)
export_directory(SCRIPT_TEST TEST_FILES)
