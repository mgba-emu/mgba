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
	scripting.c
	serialize.c
	sync.c
	thread.c
	tile-cache.c
	timing.c)

set(TEST_FILES
	test/core.c)

source_group("mCore" FILES ${SOURCE_FILES})
source_group("mCore tests" FILES ${TEST_FILES})

export_directory(CORE SOURCE_FILES)
export_directory(CORE_TEST TEST_FILES)