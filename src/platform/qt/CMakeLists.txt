set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PLATFORM_SRC)
set(QT_STATIC OFF)
set(QT_DEFINES)

if(BUILD_SDL)
	add_definitions(-DBUILD_SDL)
	if(SDL2_FOUND)
		link_directories(${SDL2_LIBDIR})
	endif()
	list(APPEND PLATFORM_LIBRARY ${SDL_LIBRARY})
	list(APPEND PLATFORM_SRC ${PLATFORM_SRC} ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-events.c ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-audio.c)
	include_directories(${SDL_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/platform/sdl)
endif()

if(POLICY CMP0071)
	cmake_policy(SET CMP0071 NEW)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT_LIBRARIES)

find_package(Qt5 COMPONENTS Core Widgets Network Multimedia)
set(QT Qt5)

if(NOT BUILD_GL AND NOT BUILD_GLES2 AND NOT BUILD_GLES3)
	message(WARNING "OpenGL is recommended to build the Qt port")
endif()

set(FOUND_QT ${${QT}Widgets_FOUND} PARENT_SCOPE)
if(NOT ${QT}Widgets_FOUND)
	message(WARNING "Cannot find Qt modules")
	return()
endif()

if(APPLE)
	if(MACOSX_SDK VERSION_GREATER 10.14)
		list(APPEND QT_DEFINES USE_SHARE_WIDGET)
	endif()

	if(Qt6Widgets_VERSION)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.14")
	elseif(Qt5Widgets_VERSION MATCHES "^5.15")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.13")
	elseif(Qt5Widgets_VERSION MATCHES "^5.1[234]")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.12")
	elseif(Qt5Widgets_VERSION MATCHES "^5.11")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.11")
	elseif(Qt5Widgets_VERSION MATCHES "^5.(9|10)")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.10")
	elseif(Qt5Widgets_VERSION MATCHES "^5.8")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.9")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.8")
	endif()
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()

get_target_property(QT_TYPE ${QT}::Core TYPE)
if(QT_TYPE STREQUAL STATIC_LIBRARY)
	set(QT_STATIC ON)
	list(APPEND QT_DEFINES QT_STATIC)
endif()

set(SOURCE_FILES
	AboutScreen.cpp
	AbstractUpdater.cpp
	Action.cpp
	ActionMapper.cpp
	ApplicationUpdater.cpp
	ApplicationUpdatePrompt.cpp
	AssetInfo.cpp
	AssetTile.cpp
	AssetView.cpp
	AudioProcessor.cpp
	CheatsModel.cpp
	CheatsView.cpp
	CheckBoxDelegate.cpp
	ConfigController.cpp
	ColorPicker.cpp
	CoreManager.cpp
	CoreController.cpp
	Display.cpp
	DisplayGL.cpp
	DisplayQt.cpp
	ForwarderController.cpp
	ForwarderGenerator.cpp
	ForwarderGenerator3DS.cpp
	ForwarderGeneratorVita.cpp
	ForwarderView.cpp
	FrameView.cpp
	GBAApp.cpp
	GBAKeyEditor.cpp
	GIFView.cpp
	IOViewer.cpp
	input/Gamepad.cpp
	input/GamepadAxisEvent.cpp
	input/GamepadButtonEvent.cpp
	input/GamepadHatEvent.cpp
	input/InputDriver.cpp
	input/InputSource.cpp
	input/InputMapper.cpp
	input/KeySource.cpp
	InputController.cpp
	InputProfile.cpp
	KeyEditor.cpp
	LoadSaveState.cpp
	LogController.cpp
	LogConfigModel.cpp
	LogView.cpp
	LogWidget.cpp
	MapView.cpp
	MemoryDump.cpp
	MemoryModel.cpp
	MemorySearch.cpp
	MemoryView.cpp
	MessagePainter.cpp
	MultiplayerController.cpp
	ObjView.cpp
	OpenGLBug.cpp
	OverrideView.cpp
	PaletteView.cpp
	PlacementControl.cpp
	RegisterView.cpp
	ReportView.cpp
	ROMInfo.cpp
	RotatedHeaderView.cpp
	SaveConverter.cpp
	SavestateButton.cpp
	SensorView.cpp
	SettingsView.cpp
	ShaderSelector.cpp
	ShortcutController.cpp
	ShortcutModel.cpp
	ShortcutView.cpp
	Swatch.cpp
	TilePainter.cpp
	TileView.cpp
	utils.cpp
	Window.cpp
	VFileDevice.cpp
	VideoProxy.cpp
	VideoView.cpp)

set(UI_FILES
	AboutScreen.ui
	ApplicationUpdatePrompt.ui
	ArchiveInspector.ui
	AssetTile.ui
	BattleChipView.ui
	CheatsView.ui
	DebuggerConsole.ui
	DolphinConnector.ui
	ForwarderView.ui
	FrameView.ui
	GIFView.ui
	IOViewer.ui
	LoadSaveState.ui
	LogView.ui
	MapView.ui
	MemoryDump.ui
	MemorySearch.ui
	MemoryView.ui
	ObjView.ui
	OverrideView.ui
	PaletteView.ui
	PlacementControl.ui
	PrinterView.ui
	ReportView.ui
	ROMInfo.ui
	SaveConverter.ui
	SensorView.ui
	SettingsView.ui
	ShaderSelector.ui
	ShortcutView.ui
	TileView.ui
	VideoView.ui)

set(GBA_SRC
	BattleChipModel.cpp
	BattleChipUpdater.cpp
	BattleChipView.cpp
	DolphinConnector.cpp
	GBAOverride.cpp)

set(GB_SRC
	GameBoy.cpp
	GBOverride.cpp
	PrinterView.cpp)

set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libqt5widgets5")

set(AUDIO_SRC)
if(BUILD_SDL)
	list(APPEND SOURCE_FILES input/SDLInputDriver.cpp)
	list(APPEND AUDIO_SRC AudioProcessorSDL.cpp)
endif()

if(M_CORE_GBA)
	list(APPEND PLATFORM_SRC ${GBA_SRC})
endif()

if(M_CORE_GB)
	list(APPEND PLATFORM_SRC ${GB_SRC})
endif()

if(${QT}Multimedia_FOUND)
	list(APPEND AUDIO_SRC
		AudioProcessorQt.cpp
		AudioDevice.cpp)
	list(APPEND SOURCE_FILES
		VideoDumper.cpp)
	if (WIN32 AND QT_STATIC)
		list(APPEND QT_LIBRARIES ${QT}::QWindowsAudioPlugin ${QT}::DSServicePlugin ${QT}::QWindowsVistaStylePlugin ${QT}::QJpegPlugin
                                 strmiids mfuuid mfplat mf ksguid dxva2 evr d3d9)
	endif()
	list(APPEND QT_LIBRARIES ${QT}::Multimedia)
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

if(USE_DISCORD_RPC)
	list(APPEND QT_LIBRARIES discord-rpc)
	list(APPEND SOURCE_FILES DiscordCoordinator.cpp)
endif()

if(ENABLE_SCRIPTING)
	list(APPEND SOURCE_FILES
		scripting/ScriptingController.cpp
		scripting/ScriptingTextBuffer.cpp
		scripting/ScriptingTextBufferModel.cpp
		scripting/ScriptingView.cpp)

	list(APPEND UI_FILES
		scripting/ScriptingView.ui)
endif()

if(TARGET Qt6::Core)
	qt_add_resources(RESOURCES resources.qrc)
else()
	qt5_add_resources(RESOURCES resources.qrc)
endif()

if(BUILD_UPDATER)
	if(DEFINED CMAKE_CONFIGURATION_TYPES)
		# Required for e.g. MSVC
		file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/updater.qrc INPUT ${CMAKE_CURRENT_SOURCE_DIR}/updater.qrc.in)
	else()
		# Required for qt_add_resources to manage dependencies properly
		# TODO: Figure out how to do this with MSVC too
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/updater-config.qrc.in ${CMAKE_CURRENT_BINARY_DIR}/updater.qrc)
	endif()
	if(TARGET Qt6::Core)
		qt_add_resources(UPDATER_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/updater.qrc)
	else()
		qt5_add_resources(UPDATER_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/updater.qrc)
	endif()
	set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/updater.qrc PROPERTIES GENERATED ON)
	list(APPEND RESOURCES ${UPDATER_RESOURCES})
endif()
if(APPLE)
	set(MACOSX_BUNDLE_ICON_FILE mgba.icns)
	set(MACOSX_BUNDLE_BUNDLE_VERSION ${LIB_VERSION_STRING})
	set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
	set(MACOSX_BUNDLE_GUI_IDENTIFIER com.endrift.${BINARY_NAME}-qt)
	set_source_files_properties(${CMAKE_SOURCE_DIR}/res/mgba.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	if(DISTBUILD)
		set(APPDIR ".")
	else()
		set(APPDIR "Applications")
	endif()
endif()
if(WIN32)
	configure_file(${CMAKE_SOURCE_DIR}/res/mgba.rc.in ${CMAKE_BINARY_DIR}/res/mgba.rc)
	list(APPEND RESOURCES ${CMAKE_BINARY_DIR}/res/mgba.rc)
	set_source_files_properties(${CMAKE_BINARY_DIR}/res/mgba.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/res/mgba.ico)
endif()
if(NOT DEFINED DATADIR)
	if(APPLE)
		set(DATADIR ${APPDIR}/${PROJECT_NAME}.app/Contents/Resources)
	elseif(WIN32 AND NOT WIN32_UNIX_PATHS)
		set(DATADIR ".")
	else()
		set(DATADIR ${CMAKE_INSTALL_DATADIR}/${BINARY_NAME})
	endif()
endif()
if(BUILD_GL OR BUILD_GLES2 OR BUILD_EPOXY)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/shaders DESTINATION ${DATADIR} COMPONENT ${BINARY_NAME}-qt)
endif()
if(ENABLE_SCRIPTING AND USE_LUA)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/scripts DESTINATION ${DATADIR} COMPONENT ${BINARY_NAME}-qt)
endif()
install(FILES ${CMAKE_SOURCE_DIR}/res/nointro.dat DESTINATION ${DATADIR} COMPONENT ${BINARY_NAME}-qt)
if(NOT WIN32 AND NOT APPLE)
	if(DATADIR MATCHES "^\\.[.\\]")
		list(APPEND QT_DEFINES DATADIR="${DATADIR}")
	else()
		list(APPEND QT_DEFINES DATADIR="${CMAKE_INSTALL_PREFIX}/${DATADIR}")
	endif()
endif()

find_package(${QT}LinguistTools)
if(${QT}LinguistTools_FOUND)
	set(TRANSLATION_FILES)
	set(TRANSLATION_QRC "${CMAKE_CURRENT_BINARY_DIR}/ts.qrc")
	file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ts/${BINARY_NAME}-*.ts")
	if(UPDATE_TRANSLATIONS)
		if(TARGET Qt6::Core)
			qt_create_translation(TRANSLATION_FILES ${SOURCE_FILES} ${AUDIO_SRC} ${PLATFORM_SRC} ${UI_FILES} ${TS_FILES} OPTIONS -locations absolute -no-obsolete -I "${CMAKE_CURRENT_SOURCE_DIR}")
		else()
			qt5_create_translation(TRANSLATION_FILES ${SOURCE_FILES} ${AUDIO_SRC} ${PLATFORM_SRC} ${UI_FILES} ${TS_FILES} OPTIONS -locations absolute -no-obsolete -I "${CMAKE_CURRENT_SOURCE_DIR}")
		endif()
		list(REMOVE_ITEM TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ts/${BINARY_NAME}-template.ts")
	else()
		list(REMOVE_ITEM TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ts/${BINARY_NAME}-template.ts")
		if(TARGET Qt6::Core)
			qt_add_translation(TRANSLATION_FILES ${TS_FILES})
		else()
			qt5_add_translation(TRANSLATION_FILES ${TS_FILES})
		endif()
	endif()
	set(QT_QM_FILES)
	if(QT_STATIC)
		get_target_property(QT_CORE_LOCATION ${QT}::Core LOCATION)
		get_filename_component(QT_CORE_LOCATION ${QT_CORE_LOCATION} DIRECTORY)
		get_filename_component(QT_QM_LOCATION "${QT_CORE_LOCATION}/../translations" ABSOLUTE)
		foreach(TS ${TS_FILES})
			get_filename_component(TS ${TS} NAME)
			string(REGEX REPLACE "${BINARY_NAME}-(.*).ts$" "qtbase_\\1.qm" QT_QM "${TS}")
			set(QT_QM "${QT_QM_LOCATION}/${QT_QM}")
			if(EXISTS "${QT_QM}")
				list(APPEND QT_QM_FILES "${QT_QM}")
			endif()
		endforeach()
		list(APPEND TRANSLATION_FILES ${QT_QM_FILES})
	endif()

	file(WRITE ${TRANSLATION_QRC} "<RCC>\n\t<qresource prefix=\"/translations/\">\n")
	foreach(TS ${TRANSLATION_FILES})
		get_filename_component(TS_BASE "${TS}" NAME)
		file(APPEND ${TRANSLATION_QRC} "\t\t<file alias=\"${TS_BASE}\">${TS}</file>\n")
	endforeach()
	file(APPEND ${TRANSLATION_QRC} "\t</qresource>\n</RCC>")

	if(TARGET Qt6::Core)
		qt_add_resources(TRANSLATION_RESOURCES ${TRANSLATION_QRC})
	else()
		qt5_add_resources(TRANSLATION_RESOURCES ${TRANSLATION_QRC})
	endif()
	list(APPEND RESOURCES ${TRANSLATION_RESOURCES})
endif()

if(TARGET Qt6::Core)
	qt_wrap_ui(UI_SRC ${UI_FILES})
else()
	qt5_wrap_ui(UI_SRC ${UI_FILES})
endif()

add_executable(${BINARY_NAME}-qt WIN32 MACOSX_BUNDLE main.cpp ${CMAKE_SOURCE_DIR}/res/mgba.icns ${SOURCE_FILES} ${PLATFORM_SRC} ${UI_SRC} ${AUDIO_SRC} ${RESOURCES})
set_target_properties(${BINARY_NAME}-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/info.plist.in COMPILE_DEFINITIONS "${FEATURE_DEFINES};${FUNCTION_DEFINES};${OS_DEFINES};${QT_DEFINES}" COMPILE_OPTIONS "${FEATURE_FLAGS}")

if(WIN32)
	set_target_properties(${BINARY_NAME}-qt PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
	if(NOT MSVC)
		target_link_libraries(${BINARY_NAME}-qt -municode)
	endif()
endif()

list(APPEND QT_LIBRARIES ${QT}::Widgets ${QT}::Network)
if(BUILD_GL OR BUILD_GLES2 OR BUILD_EPOXY)
	list(APPEND QT_LIBRARIES ${OPENGL_LIBRARY} ${OPENGLES2_LIBRARY})
endif()
if(QT_STATIC)
	find_library(QTPCRE NAMES qtpcre2 qtpcre)
	if(WIN32)
		if(CMAKE_CROSSCOMPILING)
			set(QWINDOWS_DEPS ${QT}EventDispatcherSupport ${QT}FontDatabaseSupport ${QT}ThemeSupport ${QT}WindowsUIAutomationSupport)
		endif()
		list(APPEND QT_LIBRARIES ${QT}::QWindowsIntegrationPlugin ${QWINDOWS_DEPS} amstrmid dwmapi uxtheme imm32 -static-libgcc -static-libstdc++)
		set_target_properties(${QT}::Core PROPERTIES INTERFACE_LINK_LIBRARIES "${QTPCRE};version;winmm;ssl;crypto;ws2_32;iphlpapi;crypt32;userenv;netapi32;wtsapi32")
		set_target_properties(${QT}::Gui PROPERTIES INTERFACE_LINK_LIBRARIES ${OPENGL_LIBRARY} ${OPENGLES2_LIBRARY})
	elseif(APPLE)
		find_package(Cups)
		find_package(${QT}PrintSupport)
		list(APPEND QT_LIBRARIES Cups ${QT}::PrintSupport ${QT}::QCocoaIntegrationPlugin ${QT}::CoreAudioPlugin ${QT}::AVFServicePlugin ${QT}::QCocoaPrinterSupportPlugin)
		list(APPEND QT_LIBRARIES ${QT}AccessibilitySupport ${QT}ClipboardSupport ${QT}FontDatabaseSupport ${QT}GraphicsSupport ${QT}ThemeSupport)
		if(CMAKE_SYSTEM_VERSION VERSION_LESS "19.0")
			list(APPEND QT_LIBRARIES ${QT}CglSupport)
		endif()
		list(APPEND QT_LIBRARIES "-framework AVFoundation" "-framework CoreMedia" "-framework SystemConfiguration" "-framework Security")
		set_target_properties(${QT}::Core PROPERTIES INTERFACE_LINK_LIBRARIES "${QTPCRE}")
	elseif(UNIX)
		list(APPEND QT_LIBRARIES ${QT}::FontDatabaseSupport ${QT}::XcbQpa ${QT}::QWaylandIntegrationPlugin)
	endif()
endif()
target_link_libraries(${BINARY_NAME}-qt ${PLATFORM_LIBRARY} ${BINARY_NAME} ${QT_LIBRARIES})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}" PARENT_SCOPE)

install(TARGETS ${BINARY_NAME}-qt
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${BINARY_NAME}-qt
	BUNDLE DESTINATION ${APPDIR} COMPONENT ${BINARY_NAME}-qt)
if(UNIX AND NOT APPLE)
	install(FILES ${CMAKE_SOURCE_DIR}/res/mgba-qt.desktop DESTINATION share/applications RENAME io.mgba.${PROJECT_NAME}.desktop COMPONENT ${BINARY_NAME}-qt)
endif()
if(UNIX AND NOT (APPLE AND DISTBUILD))
	install(FILES ${CMAKE_SOURCE_DIR}/doc/mgba-qt.6 DESTINATION ${MANDIR}/man6 COMPONENT ${BINARY_NAME}-qt)
endif()
if(APPLE OR WIN32)
	set_target_properties(${BINARY_NAME}-qt PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif()
if(APPLE)
	if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
		get_target_property(QTCOCOA ${QT}::QCocoaIntegrationPlugin LOCATION)
		get_target_property(COREAUDIO ${QT}::CoreAudioPlugin LOCATION)
		get_target_property(QTAVFSERVICE ${QT}::AVFServicePlugin LOCATION)
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
		install(CODE "execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/tools/deploy-mac.py\" -v ${DEPLOY_OPTIONS} \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${APPDIR}/${PROJECT_NAME}.app\")")
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
	if(DISTBUILD)
		file(WRITE "${CMAKE_BINARY_DIR}/portable.ini" "")
		install(FILES "${CMAKE_BINARY_DIR}/portable.ini" DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ${BINARY_NAME}-qt)
	endif()
endif()

if(DISTBUILD AND NOT APPLE)
	if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND "${OBJCOPY}" --only-keep-debug "$<TARGET_FILE:${BINARY_NAME}-qt>" "$<TARGET_FILE:${BINARY_NAME}-qt>.debug")
		add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND "${STRIP}" "$<TARGET_FILE:${BINARY_NAME}-qt>")
		add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND "${OBJCOPY}" --add-gnu-debuglink "$<TARGET_FILE:${BINARY_NAME}-qt>.debug" "$<TARGET_FILE:${BINARY_NAME}-qt>")
		install(FILES "$<TARGET_FILE:${BINARY_NAME}-qt>.debug" DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${BINARY_NAME}-qt-dbg)
	elseif(BUILD_STATIC AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
		add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND "${STRIP}" "$<TARGET_FILE:${BINARY_NAME}-qt>")
	endif()
endif()
