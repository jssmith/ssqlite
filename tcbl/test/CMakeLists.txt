include_directories("${PROJECT_SOURCE_DIR}/src")
add_executable(test_tcbl
        test_tcbl.c
        memvfs.h
        memvfs.c
        ../src/sglib.h
        ../src/runtime.h
        ../src/tcbl_vfs.h)
add_dependencies(test_tcbl test_tcbl)
add_test(test_tcbl test_tcbl)
target_link_libraries(test_tcbl tcbl cmocka)