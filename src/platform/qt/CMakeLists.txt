set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PLATFORM_SRC)
set(QT_STATIC OFF)

if(BUILD_SDL)
	if(NOT SDL_FOUND AND NOT SDL2_FOUND)
		find_package(SDL 1.2 REQUIRED)
	endif()
	if(SDL2_FOUND)
		link_directories(${SDL2_LIBDIR})
	endif()
	list(APPEND PLATFORM_LIBRARY ${SDL_LIBRARY})
	list(APPEND PLATFORM_SRC ${PLATFORM_SRC} ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-events.c ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-audio.c)
	include_directories(${SDL_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/platform/sdl)
endif()

if(POLICY CMP0071)
	cmake_policy(SET CMP0071 OLD)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Multimedia)
find_package(Qt5OpenGL)
find_package(Qt5Widgets)

if(NOT BUILD_GL AND NOT BUILD_GLES2)
	message(WARNING "OpenGL is recommended to build the Qt port")
endif()

if(NOT Qt5Widgets_FOUND)
	message(WARNING "Cannot find Qt modules")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

if(APPLE)
	if(Qt5Widgets_VERSION MATCHES "^5.1[0-9]")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.8")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.7")
	endif()
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
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
	ColorPicker.cpp
	CoreManager.cpp
	CoreController.cpp
	Display.cpp
	DisplayGL.cpp
	DisplayQt.cpp
	GBAApp.cpp
	GBAKeyEditor.cpp
	GIFView.cpp
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
	MapView.cpp
	MemoryModel.cpp
	MemorySearch.cpp
	MemoryView.cpp
	MessagePainter.cpp
	MultiplayerController.cpp
	ObjView.cpp
	OverrideView.cpp
	PaletteView.cpp
	PrinterView.cpp
	RegisterView.cpp
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
	utils.cpp
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
	LoadSaveState.ui
	LogView.ui
	MapView.ui
	MemorySearch.ui
	MemoryView.ui
	ObjView.ui
	OverrideView.ui
	PaletteView.ui
	PrinterView.ui
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

set(QT_LIBRARIES)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libqt5widgets5,libqt5opengl5")

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
	list(APPEND SOURCE_FILES
		VideoDumper.cpp)
	if (WIN32 AND QT_STATIC)
		list(APPEND QT_LIBRARIES Qt5::QWindowsAudioPlugin Qt5::DSServicePlugin
                                 strmiids mfuuid mfplat mf ksguid dxva2 evr d3d9)
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
		library/LibraryController.cpp
		library/LibraryGrid.cpp
		library/LibraryTree.cpp)
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
endif()
if(NOT DEFINED DATADIR)
	if(APPLE)
		set(DATADIR Applications/${PROJECT_NAME}.app/Contents/Resources)
	elseif(WIN32 AND NOT WIN32_UNIX_PATHS)
		set(DATADIR ".")
	else()
		set(DATADIR ${CMAKE_INSTALL_DATADIR}/${BINARY_NAME})
	endif()
endif()
if(BUILD_GL OR BUILD_GLES2)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/shaders DESTINATION ${DATADIR} COMPONENT ${BINARY_NAME}-qt)
endif()
install(FILES ${CMAKE_SOURCE_DIR}/res/nointro.dat DESTINATION ${DATADIR} COMPONENT ${BINARY_NAME}-qt)
if(NOT WIN32 AND NOT APPLE)
	list(APPEND QT_DEFINES DATADIR="${CMAKE_INSTALL_PREFIX}/${DATADIR}")
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
	set(QT_QM_FILES)
	if(QT_STATIC)
		get_target_property(QT_CORE_LOCATION Qt5::Core LOCATION)
		get_filename_component(QT_CORE_LOCATION ${QT_CORE_LOCATION} DIRECTORY)
		get_filename_component(QT_QM_LOCATION "${QT_CORE_LOCATION}/../translations" ABSOLUTE)
		foreach(TS ${TS_FILES})
			get_filename_component(TS ${TS} NAME)
			string(REGEX REPLACE "${BINARY_NAME}-(.*).ts$" "qtbase_\\1.qm" QT_QM "${TS}")
			list(APPEND QT_QM_FILES "${QT_QM_LOCATION}/${QT_QM}")
		endforeach()
		list(APPEND TRANSLATION_FILES ${QT_QM_FILES})
	endif()
	add_custom_command(OUTPUT ${TRANSLATION_QRC}
		               COMMAND ${CMAKE_COMMAND} -DTRANSLATION_QRC:FILEPATH="${TRANSLATION_QRC}" -DQM_BASE="${CMAKE_CURRENT_BINARY_DIR}" "-DTRANSLATION_FILES='${TRANSLATION_FILES}'" -P "${CMAKE_CURRENT_SOURCE_DIR}/ts.cmake"
		               DEPENDS ${TRANSLATION_FILES})
	qt5_add_resources(TRANSLATION_RESOURCES ${TRANSLATION_QRC})
	list(APPEND RESOURCES ${TRANSLATION_RESOURCES})
endif()

qt5_wrap_ui(UI_SRC ${UI_FILES})

add_executable(${BINARY_NAME}-qt WIN32 MACOSX_BUNDLE main.cpp ${CMAKE_SOURCE_DIR}/res/mgba.icns ${SOURCE_FILES} ${PLATFORM_SRC} ${UI_SRC} ${AUDIO_SRC} ${RESOURCES})
set_target_properties(${BINARY_NAME}-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/info.plist.in COMPILE_DEFINITIONS "${FEATURE_DEFINES};${FUNCTION_DEFINES};${OS_DEFINES};${QT_DEFINES}")

list(APPEND QT_LIBRARIES Qt5::Widgets)
if(BUILD_GL OR BUILD_GLES2)
	list(APPEND QT_LIBRARIES Qt5::OpenGL ${OPENGL_LIBRARY} ${OPENGLES2_LIBRARY})
endif()
if(WIN32 AND QT_STATIC)
	list(APPEND QT_LIBRARIES qwindows dwmapi imm32 uxtheme Qt5EventDispatcherSupport Qt5FontDatabaseSupport Qt5ThemeSupport)
	set_target_properties(Qt5::Core PROPERTIES INTERFACE_LINK_LIBRARIES "qtpcre2;version;winmm;ws2_32")
endif()
target_link_libraries(${BINARY_NAME}-qt ${PLATFORM_LIBRARY} ${BINARY_NAME} ${QT_LIBRARIES})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}" PARENT_SCOPE)

install(TARGETS ${BINARY_NAME}-qt
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${BINARY_NAME}-qt
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
	if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
		get_target_property(QTCOCOA Qt5::QCocoaIntegrationPlugin LOCATION)
		get_target_property(COREAUDIO Qt5::CoreAudioPlugin LOCATION)
		get_target_property(QTAVFSERVICE Qt5::AVFServicePlugin LOCATION)
		set(BUNDLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.app)
		target_sources(${BINARY_NAME}-qt PRIVATE "${PLUGINS}")
		set_source_files_properties("${QTCOCOA}" PROPERTIES MACOSX_PACKAGE_LOCATION Contents/PlugIns)
		set_source_files_properties("${COREAUDIO}" PROPERTIES MACOSX_PACKAGE_LOCATION Contents/PlugIns)
		set_source_files_properties("${QTAVFSERVICE}" PROPERTIES MACOSX_PACKAGE_LOCATION Contents/PlugIns)
		install(CODE "
		   include(BundleUtilities)
		   set(BU_CHMOD_BUNDLE_ITEMS ON)
		   file(GLOB_RECURSE PLUGINS \"${BUNDLE_PATH}/Contents/PlugIns/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
		   fixup_bundle(\"${BUNDLE_PATH}\"   \"${PLUGINS}\"   \"\")
		   " COMPONENT ${BINARY_NAME}-qt)
	else()
		set(DEPLOY_OPTIONS -p platforms/libqcocoa.dylib,audio/libqtaudio_coreaudio.dylib,mediaservice/libqavfcamera.dylib)
		if(NOT CMAKE_INSTALL_NAME_TOOL EQUAL "install_name_tool")
			set(DEPLOY_OPTIONS ${DEPLOY_OPTIONS} -I "${CMAKE_INSTALL_NAME_TOOL}")
		endif()
		if(DEFINED CMAKE_OTOOL AND NOT CMAKE_OTOOL EQUAL "otool")
			set(DEPLOY_OPTIONS ${DEPLOY_OPTIONS} -O "${CMAKE_OTOOL}")
		endif()
		if(DEFINED CROSS_ROOT)
			set(DEPLOY_OPTIONS ${DEPLOY_OPTIONS} -R "${CROSS_ROOT}")
		endif()
		install(CODE "execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/tools/deploy-mac.py\" -v ${DEPLOY_OPTIONS} \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/Applications/${PROJECT_NAME}.app\")")
	endif()
elseif(WIN32)
	if(CMAKE_MAJOR_VERSION EQUAL 3 AND CMAKE_MINOR_VERSION EQUAL 8)
		# Work around CMake issue #16907
		set_target_properties(${BINARY_NAME}-qt PROPERTIES AUTORCC ON SKIP_AUTORCC ON)
	endif()
	if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
		find_program(BASH bash)
		install(CODE "execute_process(COMMAND \"${BASH}\" \"${CMAKE_SOURCE_DIR}/tools/deploy-win.sh\" \"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.exe\" \"\${CMAKE_INSTALL_PREFIX}\" \"\$ENV{PWD}\" WORKING_DIRECTORY \"${CMAKE_BINARY_DIR}\")" COMPONENT ${BINARY_NAME}-qt)
	endif()
endif()
