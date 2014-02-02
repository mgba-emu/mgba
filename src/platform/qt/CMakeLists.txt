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
find_package(OpenGL REQUIRED)

set(SOURCE_FILES AudioDevice.cpp Display.cpp GameController.cpp Window.cpp)

if(USE_GDB_STUB)
	set(SOURCE_FILES ${SOURCE_FILES} GDBController.cpp GDBWindow.cpp)
endif()

add_executable(QGBAc WIN32 MACOSX_BUNDLE main.cpp ${SOURCE_FILES})

qt5_use_modules(QGBAc Widgets Multimedia OpenGL)
target_link_libraries(QGBAc ${OPENGL_LIBRARY} ${BINARY_NAME})
