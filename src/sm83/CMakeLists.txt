include(ExportDirectory)
set(SOURCE_FILES
	decoder.c
	isa-sm83.c
	sm83.c)

set(DEBUGGER_FILES
	debugger/cli-debugger.c
	debugger/debugger.c
	debugger/memory-debugger.c)

source_group("SM83 core" FILES ${SOURCE_FILES})
source_group("SM83 debugger" FILES ${DEBUGGER_FILES})

export_directory(SM83 SOURCE_FILES)
export_directory(SM83_DEBUGGER DEBUGGER_FILES)
