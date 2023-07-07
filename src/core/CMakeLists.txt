include(ExportDirectory)
set(SOURCE_FILES
	bitmap-cache.c
	cache-set.c
	cheats.c
	config.c
	core.c
	directories.c
	input.c
	interface.c
	library.c
	lockstep.c
	log.c
	map-cache.c
	mem-search.c
	rewind.c
	serialize.c
	sync.c
	thread.c
	tile-cache.c
	timing.c)

set(TEST_FILES
	test/core.c)

if(ENABLE_SCRIPTING)
	set(SCRIPTING_FILES
		scripting.c)

	if(USE_LUA)
		list(APPEND TEST_FILES
			test/scripting.c)
	endif()
endif()

source_group("mCore" FILES ${SOURCE_FILES})
source_group("mCore scripting" FILES ${SCRIPTING_FILES})
source_group("mCore tests" FILES ${TEST_FILES})

export_directory(CORE SOURCE_FILES)
export_directory(CORE_SCRIPT SCRIPTING_FILES)
export_directory(CORE_TEST TEST_FILES)
