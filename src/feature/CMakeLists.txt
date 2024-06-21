include(ExportDirectory)
set(SOURCE_FILES
	commandline.c
	thread-proxy.c
	updater.c
	video-backend.c
	video-logger.c)

set(GUI_FILES
	gui/cheats.c
	gui/gui-config.c
	gui/gui-runner.c
	gui/remap.c)

source_group("Extra features" FILES ${SOURCE_FILES})
source_group("Extra GUI source" FILES ${GUI_FILES})

export_directory(EXTRA SOURCE_FILES)
export_directory(EXTRA_GUI GUI_FILES)
