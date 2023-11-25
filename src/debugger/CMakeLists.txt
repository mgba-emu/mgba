include(ExportDirectory)
set(SOURCE_FILES
	access-logger.c
	cli-debugger.c
	debugger.c
	parser.c
	symbols.c
	stack-trace.c)

if(ENABLE_SCRIPTING)
	list(APPEND SOURCE_FILES cli-debugger-scripting.c)
endif()

if(USE_EDITLINE)
	list(APPEND SOURCE_FILES cli-el-backend.c)
endif()

if(USE_GDB_STUB)
	list(APPEND SOURCE_FILES gdb-stub.c)
endif()

set(TEST_FILES
	test/lexer.c
	test/parser.c)

source_group("Debugger" FILES ${SOURCE_FILES})
source_group("Debugger tests" FILES ${TEST_FILES})

export_directory(DEBUGGER SOURCE_FILES)
export_directory(DEBUGGER_TEST TEST_FILES)
