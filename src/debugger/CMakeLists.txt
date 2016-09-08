set(SRC
	debugger.c
	parser.c
	)

set(RENDERER_SRC
	renderers/software-bg.c
	renderers/software-mode0.c
	renderers/software-obj.c
	renderers/video-software.c
	)

if(USE_CLI_DEBUGGER)
	list(APPEND SRC cli-debugger.c)
endif()

if(USE_GDB_STUB)
	list(APPEND SRC gdb-stub.c)
endif()

source_group("Debugger source" FILES ${SRC})
add_internal_library(debugger "${SRC}" "")
