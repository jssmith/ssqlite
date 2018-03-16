find_package (Threads)

include_directories("${PROJECT_SOURCE_DIR}/src")
add_executable(test_tcbl
        test_tcbl.h
        test_tcbl.c
        memvfs.h
        memvfs.c
        unixvfs.h
        unixvfs.c
        ../src/sglib.h
        ../src/runtime.h
        ../src/tcbl_vfs.h
        change_fh.c
        change_fh.h)
add_dependencies(test_tcbl test_tcbl)
add_test(test_tcbl test_tcbl)
target_link_libraries(test_tcbl tcbl cmocka ${CMAKE_THREAD_LIBS_INIT})

include_directories(BEFORE /Users/johann/d/sqlite-autoconf-3220000)
add_library(sqlite3tcbl SHARED
        sqlitetcbl.c
        memvfs.h
        memvfs.c
        unixvfs.h
        unixvfs.c
        ../src/tcbl_vfs.c
        ../src/tcbl_vfs.h
        ../src/runtime.h
        ../src/sglib.h)

add_library(vfslog SHARED vfslog.c)