Building \_sqlite3.so
====================

As provided, Lambda's Python 3.6 runtime does not support SQLite because the Python Module `sqlite3` is not present.
The module can be added simply by dropping the `_sqlite3.so` from a standard Python distribution into the into the code deployed, but this still pulls in a system sqlite3 installation, which in this case is version 3.?.?, dating to 2013.
We modify the Python build process to include SQLite in the same library as the `sqlite3` module.

## Download sources

Get a current SQLite source.

```
wget https://sqlite.org/2017/sqlite-amalgamation-3210000.zip
tar xzvf sqlite-amalgamation-3210000.zip
```

Get a the appropriate Python source

```
wget https://www.python.org/ftp/python/3.6.1/Python-3.6.1.tgz
tar xzvf Python-3.6.1.tgz
```

## Patch Python

This [diff](python-3.6.1.diff) applies to `setup.py`, causing for SQLite to be statically included in the `sqlite3` Python module.

```
cd $PATH_TO_PYTHON_SOURCE
patch -p0 setup.py < $PATH_TO_SSQLITE/python-3.6.1.diff
```

Copy over sqlite files
```
cp $PATH_TO_SQLITE_SOURCE/sqlite3.c \
    $PATH_TO_PYTHON_SOURCE/Modules/_sqlite
cp $PATH_TO_SQLITE_SOURCE/sqlite3.h \
    $PATH_TO_PYTHON_SOURCE/Modules/_sqlite
```

Build the project
```
./configure
make
```

Copy the sqlite3 Python module to the Lambda distribution
```
cp build/lib.linux-x86_64-3.6/_sqlite3.cpython-36m-x86_64-linux-gnu.so \
    $PATH_TO_SSQLITE/dist/_sqlite3.so
```
