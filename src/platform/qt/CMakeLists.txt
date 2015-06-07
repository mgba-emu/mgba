cmake_minimum_required(VERSION 2.8.11)
enable_language(CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

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

find_package(Qt5Multimedia)
find_package(Qt5OpenGL)
find_package(Qt5Widgets)
find_package(OpenGL)

if(NOT Qt5OpenGL_FOUND OR NOT Qt5Widgets_FOUND OR NOT OPENGL_FOUND)
	message(WARNING "Cannot find Qt modules")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

list(APPEND PLATFORM_SRC ${PLATFORM_SRC} ${CMAKE_SOURCE_DIR}/src/platform/opengl/gl.c)

get_target_property(QT_TYPE Qt5::Core TYPE)
if(QT_TYPE STREQUAL STATIC_LIBRARY)
	set(QT_STATIC ON)
	add_definitions(-DQT_STATIC)
endif()

set(SOURCE_FILES
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
	InputController.cpp
	KeyEditor.cpp
	LoadSaveState.cpp
	LogView.cpp
	MemoryModel.cpp
	MemoryView.cpp
	MultiplayerController.cpp
	OverrideView.cpp
	PaletteView.cpp
	SavestateButton.cpp
	SensorView.cpp
	SettingsView.cpp
	ShortcutController.cpp
	ShortcutView.cpp
	Swatch.cpp
	Window.cpp
	VFileDevice.cpp
	VideoView.cpp)

qt5_wrap_ui(UI_FILES
	CheatsView.ui
	GIFView.ui
	LoadSaveState.ui
	LogView.ui
	MemoryView.ui
	OverrideView.ui
	PaletteView.ui
	SensorView.ui
	SettingsView.ui
	ShortcutView.ui
	VideoView.ui)

set(QT_LIBRARIES)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libqt5widgets5,libqt5opengl5")

set(AUDIO_SRC)
if(BUILD_SDL)
	list(APPEND AUDIO_SRC AudioProcessorSDL.cpp)
endif()

if(Qt5Multimedia_FOUND)
	list(APPEND AUDIO_SRC
		AudioProcessorQt.cpp
		AudioDevice.cpp)
	if (WIN32 AND QT_STATIC)
		list(APPEND QT_LIBRARIES qtaudio_windows strmiids winmm)
	endif()
	list(APPEND QT_LIBRARIES Qt5::Multimedia)
	add_definitions(-DBUILD_QT_MULTIMEDIA)
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS},libqt5multimedia5")
endif()

if(NOT AUDIO_SRC)
	message(WARNING "No supported audio modules found")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

if(USE_GDB_STUB)
	list(APPEND PLATFORM_SRC GDBController.cpp GDBWindow.cpp)
endif()
set(MACOSX_BUNDLE_ICON_FILE mgba.icns)
set(MACOSX_BUNDLE_BUNDLE_VERSION ${LIB_VERSION_STRING})
set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
set(MACOSX_BUNDLE_GUI_IDENTIFIER com.endrift.${BINARY_NAME}-qt)
set_source_files_properties(${CMAKE_SOURCE_DIR}/res/mgba.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

qt5_add_resources(RESOURCES resources.qrc)
if(WIN32)
	configure_file(${CMAKE_SOURCE_DIR}/res/mgba.rc.in ${CMAKE_BINARY_DIR}/res/mgba.rc)
	list(APPEND RESOURCES ${CMAKE_BINARY_DIR}/res/mgba.rc)
	if(QT_STATIC)
		list(APPEND QT_LIBRARIES qwindows imm32)
	endif()
endif()
add_executable(${BINARY_NAME}-qt WIN32 MACOSX_BUNDLE main.cpp ${CMAKE_SOURCE_DIR}/res/mgba.icns ${SOURCE_FILES} ${PLATFORM_SRC} ${UI_FILES} ${AUDIO_SRC} ${RESOURCES})
set_target_properties(${BINARY_NAME}-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/info.plist.in COMPILE_DEFINITIONS "${FEATURE_DEFINES}")

list(APPEND QT_LIBRARIES Qt5::Widgets Qt5::OpenGL)
target_link_libraries(${BINARY_NAME}-qt ${PLATFORM_LIBRARY} ${OPENGL_LIBRARY} ${BINARY_NAME} ${QT_LIBRARIES})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}" PARENT_SCOPE)

install(TARGETS ${BINARY_NAME}-qt
	RUNTIME DESTINATION bin COMPONENT ${BINARY_NAME}-qt
	BUNDLE DESTINATION Applications COMPONENT ${BINARY_NAME}-qt)
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
