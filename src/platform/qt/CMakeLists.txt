cmake_minimum_required(VERSION 2.8.8)
project(QGBAc)

set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra --std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra --std=c++11")

if(APPLE)
	set(CMAKE_PREFIX_PATH "/usr/local/opt/qt5") # Temporary stopgap measure
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets REQUIRED)

set(UI_FILES Window.ui)
set(SOURCE_FILES Display.cpp GameController.cpp Window.cpp)

qt5_wrap_ui(UI_HEADERS ${UI_FILES})

add_executable(QGBAc WIN32 MACOSX_BUNDLE ${UI_FILES} ${UI_HEADERS} main.cpp ${SOURCE_FILES})

qt5_use_modules(QGBAc Widgets OpenGL)
target_link_libraries(QGBAc ${OPENGL_LIBRARY} ${BINARY_NAME})
