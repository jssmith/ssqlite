cmake_minimum_required(VERSION 2.8)
project(tcbl)

set(CMAKE_C_STANDARD 99)

include_directories("/usr/local/include")
link_directories("/usr/local/lib")

enable_testing()

set(HEADER_FILES src/runtime.h src/tcbl_vfs.h)

add_subdirectory(src)
add_subdirectory(test)



#set(SOURCE_FILES test/main.c)
#add_executable(tcbl ${SOURCE_FILES})
#target_link_libraries(tcbl cmocka)