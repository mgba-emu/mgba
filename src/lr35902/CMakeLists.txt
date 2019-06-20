include(ExportDirectory)
set(SOURCE_FILES
	decoder.c
	isa-lr35902.c
	lr35902.c)

set(DEBUGGER_FILES
	debugger/cli-debugger.c
	debugger/debugger.c
	debugger/memory-debugger.c)

source_group("LR35902 core" FILES ${SOURCE_FILES})
source_group("ARM LR35902" FILES ${DEBUGGER_FILES})

export_directory(LR35902 SOURCE_FILES)
export_directory(LR35902_DEBUGGER DEBUGGER_FILES)