cmake_minimum_required(VERSION 2.8)
project(tcbl)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-std=c99 -D_XOPEN_SOURCE=500 -Wall -g -O0")
set(CMAKE_MACOSX_RPATH 1)

include_directories("/usr/local/include")
link_directories("/usr/local/lib")

enable_testing()

set(HEADER_FILES src/runtime.h src/tcbl_vfs.h)

add_subdirectory(src)
add_subdirectory(test)
