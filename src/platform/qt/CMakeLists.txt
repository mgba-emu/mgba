cmake_minimum_required(VERSION 2.8.11)
enable_language(CXX)

if(NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.7")
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
endif()

set(PLATFORM_SRC)
set(QT_STATIC OFF)

if(BUILD_SDL)
	if(NOT SDL_FOUND AND NOT SDL2_FOUND)
		find_package(SDL 1.2 REQUIRED)
	endif()
	if(SDL2_FOUND)
		link_directories(${SDL2_LIBDIR})
	endif()
	list(APPEND PLATFORM_LIBRARY ${SDL_LIBRARY} ${SDLMAIN_LIBRARY})
	list(APPEND PLATFORM_SRC ${PLATFORM_SRC} ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-events.c ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-audio.c)
	include_directories(${SDL_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/platform/sdl)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT WIN32 OR NOT BUILD_SDL)
	find_package(Qt5Multimedia)
endif()
find_package(Qt5OpenGL)
find_package(Qt5Network)
find_package(Qt5Widgets)

if(NOT BUILD_GL AND NOT BUILD_GLES2)
	message(WARNING "OpenGL is required to build the Qt port")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

if(NOT Qt5OpenGL_FOUND OR NOT Qt5Widgets_FOUND OR NOT Qt5Network_FOUND)
	message(WARNING "Cannot find Qt modules")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

if(BUILD_GL)
	list(APPEND PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/opengl/gl.c)
	if(NOT WIN32 OR USE_EPOXY)
		list(APPEND PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/opengl/gles2.c)
	endif()
endif()

if(BUILD_GLES2)
	list(APPEND PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/opengl/gles2.c)
endif()

get_target_property(QT_TYPE Qt5::Core TYPE)
if(QT_TYPE STREQUAL STATIC_LIBRARY)
	set(QT_STATIC ON)
	list(APPEND QT_DEFINES QT_STATIC)
endif()

set(SOURCE_FILES
	AboutScreen.cpp
	AssetTile.cpp
	AssetView.cpp
	AudioProcessor.cpp
	CheatsModel.cpp
	CheatsView.cpp
	ConfigController.cpp
	Display.cpp
	DisplayGL.cpp
	DisplayQt.cpp
	GBAApp.cpp
	GBAKeyEditor.cpp
	GIFView.cpp
	GameController.cpp
	GamepadAxisEvent.cpp
	GamepadButtonEvent.cpp
	GamepadHatEvent.cpp
	IOViewer.cpp
	InputController.cpp
	InputProfile.cpp
	KeyEditor.cpp
	LoadSaveState.cpp
	LogController.cpp
	LogView.cpp
	MemoryModel.cpp
	MemoryView.cpp
	MessagePainter.cpp
	MultiplayerController.cpp
	ObjView.cpp
	OverrideView.cpp
	PaletteView.cpp
	ROMInfo.cpp
	SavestateButton.cpp
	SensorView.cpp
	SettingsView.cpp
	ShaderSelector.cpp
	ShortcutController.cpp
	ShortcutView.cpp
	Swatch.cpp
	TilePainter.cpp
	TileView.cpp
	Window.cpp
	VFileDevice.cpp
	VideoView.cpp)

set(UI_FILES
	AboutScreen.ui
	ArchiveInspector.ui
	AssetTile.ui
	CheatsView.ui
	DebuggerConsole.ui
	GIFView.ui
	IOViewer.ui
	LibraryView.ui
	LoadSaveState.ui
	LogView.ui
	MemoryView.ui
	ObjView.ui
	OverrideView.ui
	PaletteView.ui
	ROMInfo.ui
	SensorView.ui
	SettingsView.ui
	ShaderSelector.ui
	ShortcutView.ui
	TileView.ui
	VideoView.ui)

set(GBA_SRC
	GBAOverride.cpp)

set(GB_SRC
	GBOverride.cpp)

qt5_wrap_ui(UI_SRC ${UI_FILES})

set(QT_LIBRARIES)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libqt5widgets5,libqt5opengl5,libqt5network5")

set(AUDIO_SRC)
if(BUILD_SDL)
	list(APPEND AUDIO_SRC AudioProcessorSDL.cpp)
endif()

if(M_CORE_GBA)
	list(APPEND PLATFORM_SRC ${GBA_SRC})
endif()

if(M_CORE_GB)
	list(APPEND PLATFORM_SRC ${GB_SRC})
endif()

set(QT_DEFINES)
if(Qt5Multimedia_FOUND)
	list(APPEND AUDIO_SRC
		AudioProcessorQt.cpp
		AudioDevice.cpp)
	if (WIN32 AND QT_STATIC)
		list(APPEND QT_LIBRARIES qtaudio_windows strmiids winmm)
	endif()
	list(APPEND QT_LIBRARIES Qt5::Multimedia)
	list(APPEND QT_DEFINES BUILD_QT_MULTIMEDIA)
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libqt5multimedia5")
endif()

if(NOT AUDIO_SRC)
	message(WARNING "No supported audio modules found")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

if(USE_DEBUGGERS)
	list(APPEND SOURCE_FILES
		DebuggerController.cpp
		DebuggerConsole.cpp
		DebuggerConsoleController.cpp)
endif()

if(USE_GDB_STUB)
	list(APPEND SOURCE_FILES GDBController.cpp GDBWindow.cpp)
endif()

if(USE_SQLITE3)
	list(APPEND SOURCE_FILES
		ArchiveInspector.cpp
		LibraryModel.cpp
		LibraryView.cpp)
endif()

qt5_add_resources(RESOURCES resources.qrc)
if(APPLE)
	set(MACOSX_BUNDLE_ICON_FILE mgba.icns)
	set(MACOSX_BUNDLE_BUNDLE_VERSION ${LIB_VERSION_STRING})
	set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
	set(MACOSX_BUNDLE_GUI_IDENTIFIER com.endrift.${BINARY_NAME}-qt)
	set_source_files_properties(${CMAKE_SOURCE_DIR}/res/mgba.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()
if(WIN32)
	configure_file(${CMAKE_SOURCE_DIR}/res/mgba.rc.in ${CMAKE_BINARY_DIR}/res/mgba.rc)
	list(APPEND RESOURCES ${CMAKE_BINARY_DIR}/res/mgba.rc)
	set_source_files_properties(${CMAKE_BINARY_DIR}/res/mgba.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/res/mgba.ico)
	if(QT_STATIC)
		list(APPEND QT_LIBRARIES qwindows Qt5PlatformSupport imm32)
	endif()
endif()
if(NOT DEFINED DATADIR)
	if(APPLE)
		set(DATADIR Applications/${PROJECT_NAME}.app/Contents/Resources)
	else()
		set(DATADIR ${CMAKE_INSTALL_DATADIR}/${BINARY_NAME})
	endif()
endif()
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/shaders DESTINATION ${DATADIR} COMPONENT ${BINARY_NAME}-qt)
install(FILES ${CMAKE_SOURCE_DIR}/res/nointro.dat DESTINATION ${DATADIR} COMPONENT ${BINARY_NAME}-qt)
if(NOT WIN32 AND NOT APPLE)
	list(APPEND QT_DEFINES DATADIR="${DATADIR}")
endif()

find_package(Qt5LinguistTools)
if(Qt5LinguistTools_FOUND)
	set(TRANSLATION_FILES)
	set(TRANSLATION_QRC "${CMAKE_CURRENT_BINARY_DIR}/ts.qrc")
	file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ts/${BINARY_NAME}-*.ts")
	if(UPDATE_TRANSLATIONS)
		qt5_create_translation(TRANSLATION_FILES ${SOURCE_FILES} ${UI_FILES} ${TS_FILES} OPTIONS -locations absolute -no-obsolete)
	else()
		qt5_add_translation(TRANSLATION_FILES ${TS_FILES})
	endif()
	add_custom_command(OUTPUT ${TRANSLATION_QRC}
		               COMMAND ${CMAKE_COMMAND} -DTRANSLATION_QRC:FILEPATH="${TRANSLATION_QRC}" -DQM_BASE="${CMAKE_CURRENT_BINARY_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/ts.cmake"
		               DEPENDS ${TRANSLATION_FILES})
	qt5_add_resources(TRANSLATION_RESOURCES ${TRANSLATION_QRC})
	list(APPEND RESOURCES ${TRANSLATION_RESOURCES})
endif()

add_executable(${BINARY_NAME}-qt WIN32 MACOSX_BUNDLE main.cpp ${CMAKE_SOURCE_DIR}/res/mgba.icns ${SOURCE_FILES} ${PLATFORM_SRC} ${UI_SRC} ${AUDIO_SRC} ${RESOURCES})
set_target_properties(${BINARY_NAME}-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/info.plist.in COMPILE_DEFINITIONS "${FEATURE_DEFINES};${FUNCTION_DEFINES};${OS_DEFINES};${QT_DEFINES}")

list(APPEND QT_LIBRARIES Qt5::Widgets Qt5::OpenGL Qt5::Network)
target_link_libraries(${BINARY_NAME}-qt ${PLATFORM_LIBRARY} ${BINARY_NAME} ${QT_LIBRARIES} ${OPENGL_LIBRARY} ${OPENGLES2_LIBRARY})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}" PARENT_SCOPE)

install(TARGETS ${BINARY_NAME}-qt
	RUNTIME DESTINATION bin COMPONENT ${BINARY_NAME}-qt
	BUNDLE DESTINATION Applications COMPONENT ${BINARY_NAME}-qt)
if(UNIX AND NOT APPLE)
	find_program(DESKTOP_FILE_INSTALL desktop-file-install)
	if(DESKTOP_FILE_INSTALL)
		install(CODE "execute_process(COMMAND ${DESKTOP_FILE_INSTALL} \"${CMAKE_SOURCE_DIR}/res/mgba-qt.desktop\" --dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications/\")")
	endif()
endif()
if(UNIX)
	install(FILES ${CMAKE_SOURCE_DIR}/doc/mgba-qt.6 DESTINATION ${MANDIR}/man6 COMPONENT ${BINARY_NAME}-qt)
endif()
if(APPLE OR WIN32)
	set_target_properties(${BINARY_NAME}-qt PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif()
if(APPLE)
	set(DEPLOY_OPTIONS -p platforms/libqcocoa.dylib,audio/libqtaudio_coreaudio.dylib)
	if(NOT CMAKE_INSTALL_NAME_TOOL EQUAL "install_name_tool")
		set(DEPLOY_OPTIONS ${DEPLOY_OPTIONS} -I ${CMAKE_INSTALL_NAME_TOOL})
	endif()
	if(DEFINED CMAKE_OTOOL AND NOT CMAKE_OTOOL EQUAL "otool")
		set(DEPLOY_OPTIONS ${DEPLOY_OPTIONS} -O ${CMAKE_OTOOL})
	endif()
	if(DEFINED CROSS_ROOT)
		set(DEPLOY_OPTIONS ${DEPLOY_OPTIONS} -R ${CROSS_ROOT})
	endif()
	add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND ${CMAKE_SOURCE_DIR}/tools/deploy-mac.py ${DEPLOY_OPTIONS} ${PROJECT_NAME}.app)
endif()
