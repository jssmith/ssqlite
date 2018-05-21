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
        test_runtime.h
        test_util.c
        change_fh.c
        change_fh.h
        test_runtime.h
        ../src/cachedvfs.h
        ../src/cachedvfs.c)
add_dependencies(test_tcbl test_tcbl)
add_test(test_tcbl test_tcbl)
target_link_libraries(test_tcbl tcbl cmocka ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_ll_log
        ../src/ll_log.h
        ../src/ll_log.c
        memvfs.h
        memvfs.c
        test_runtime.h
        test_util.c
        ../src/runtime.h
        test_ll_log.c
        ../src/cachedvfs.h
        ../src/cachedvfs.c)
add_test(test_ll_log test_ll_log)
target_link_libraries(test_ll_log tcbl cmocka ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_cache
        memvfs.h
        memvfs.c
        test_runtime.h
        ../src/runtime.h
        test_cache.c
        test_util.c
        ../src/cachedvfs.h
        ../src/cachedvfs.c)
add_test(test_cache test_cache)
target_link_libraries(test_cache tcbl cmocka ${CMAKE_THREAD_LIBS_INIT})

include_directories(BEFORE /Users/johann/d/sqlite-autoconf-3220000)
add_library(sqlite3tcbl SHARED
        sqlitetcbl.c
        memvfs.h
        memvfs.c
        unixvfs.h
        unixvfs.c
        ../src/vfs.h
        ../src/tcbl_vfs.c
        ../src/tcbl_vfs.h
        ../src/ll_log.h
        ../src/ll_log.c
        ../src/runtime.h
        ../src/sglib.h
        ../src/cachedvfs.h
        ../src/cachedvfs.c
        ../src/perfstats.h
        ../src/perfstats.c
        )

add_library(vfslog SHARED vfslog.c)