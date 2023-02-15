include(ExportDirectory)
set(SOURCE_FILES
	context.c
	socket.c
	stdlib.c
	types.c)

set(TEST_FILES
	test/classes.c
	test/types.c)

if(USE_LUA)
	list(APPEND SOURCE_FILES engines/lua.c)
	list(APPEND TEST_FILES
		test/stdlib.c
		test/lua.c)
endif()

source_group("Scripting" FILES ${SOURCE_FILES})
source_group("Scripting tests" FILES ${TEST_FILES})

export_directory(SCRIPT SOURCE_FILES)
export_directory(SCRIPT_TEST TEST_FILES)
