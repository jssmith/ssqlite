## Write tests

Set up the `SQLITE_EXE` environment variable

```
export SQLITE_EXE={ PATH TO SQLITE3 }
```

replacing `{ PATH TO SQLITE3 }` with the path to your sqlite executable.

Run the tests in normal execution mode (PRAGMA journal_mode=delete)

```
./test/write_tests.sh unix $(mktemp) delete
```

Run the tests in WAL mode

```
./test/write_tests.sh unix $(mktemp) wal
```

Run the tests with nfs4: TODO - fix needed here to load up the nfs4 extension.
