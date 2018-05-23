cmake_minimum_required(VERSION 2.8)
project(tcbl)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-std=gnu99 -D_XOPEN_SOURCE=500 -Wall -g -fPIC -O0")
set(CMAKE_MACOSX_RPATH 1)

include_directories("/usr/local/include" "../nfsv4/")
link_directories("/usr/local/lib" "/ssqlite/nfsv4/")

enable_testing()

set(HEADER_FILES src/tcbl_runtime.h src/tcbl_vfs.h)

#add_library(nfs4 SHARED IMPORTED)
#set_property(TARGET nfs4 PROPERTY IMPORTED_LOCATION "./nfs4.so")
#set_property(TARGET nfs4 PROPERTY IMPORTED_LOCATION "/ssqlite/nfsv4/nfs4.so")



add_subdirectory(src)
add_subdirectory(test)
