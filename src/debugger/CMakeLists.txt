include(ExportDirectory)
set(SOURCE_FILES
	cli-debugger.c
	debugger.c
	parser.c
	symbols.c
	stack-trace.c)

if(ENABLE_SCRIPTING)
	list(APPEND SOURCE_FILES cli-debugger-scripting.c)
endif()

set(TEST_FILES
	test/lexer.c
	test/parser.c)

source_group("Debugger" FILES ${SOURCE_FILES})
source_group("Debugger tests" FILES ${TEST_FILES})

export_directory(DEBUGGER SOURCE_FILES)
export_directory(DEBUGGER_TEST TEST_FILES)
