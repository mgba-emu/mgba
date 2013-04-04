cmake_minimum_required(VERSION 2.6)
project(GBAc)
file(GLOB SOURCES *.c)
add_executable(gbac ${SOURCES})
