from distutils.core import setup, Extension

module_vfsbench = Extension("vfsbench", sources = ["vfsbench.c"],
    extra_compile_args = ["-std=gnu99","-I/build","-DTIMING_DETAIL"],
    extra_link_args = ["/build/.libs/sqlite3.o"])
    #extra_link_args = ["-lsqlite3","-L/build/.libs"])

setup(name = "Sqlite3VFSBench",
        version = "1.0",
        description = "SQLite VFS Benchmark",
        ext_modules = [module_vfsbench])
