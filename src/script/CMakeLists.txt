include(ExportDirectory)
set(SOURCE_FILES
	context.c
	types.c)

set(TEST_FILES
	test/types.c)

source_group("Scripting" FILES ${SOURCE_FILES})
source_group("Scripting tests" FILES ${TEST_FILES})

export_directory(SCRIPT SOURCE_FILES)
export_directory(SCRIPT_TEST TEST_FILES)
