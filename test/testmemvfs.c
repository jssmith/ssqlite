#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

/*
 * Demonstration of the in-memory database (memvfs) with vfs tracing.
 *
 * Running this demo requires building the sqlite3.so library with.
 * test_vfstrace, i.e., add -DSQLITE_ENABLE_VFSTRACE and
 * ../sqlite-src-3210000/src/test_vfstrace.c to the build command.
 *
 * It also requires building the memvfs extension, e.g.:
 *     gcc -I "." -g -O2 -fPIC -shared -o memvfs.so \
 *     ../sqlite-src-3210000/ext/misc/memvfs.c
 */

typedef struct MemDB {
    char* data;
    size_t size;
} MemDB;


static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    int i;
    for(i=0; i<argc; i++){
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}


void usage() {
    fprintf(stderr, "Usage testmem database_file");
}

MemDB load_file(char* fn) {
    FILE* f = fopen(fn, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    MemDB memdb;
    memdb.data = malloc(fsize);
    memdb.size = fsize;
    fread(memdb.data, fsize, 1, f);
    fclose(f);
    return memdb;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        usage();
        exit(1);
    }
    char* database_file = argv[1];

    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    rc = sqlite3_open(":memory:", &db);
    if (rc) {
        fprintf(stderr, "Unable to open initialization database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return (1);
    }

    rc = sqlite3_enable_load_extension(db, 1);
    if (rc) {
        fprintf(stderr, "Unable to enable extension loading");
        exit(1);
    }

    rc = sqlite3_load_extension(db, "./memvfs", 0, &zErrMsg);
    if (rc) {
        fprintf(stderr, "Unable to load extension %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(1);
    }

    rc = sqlite3_close(db);
    if (rc) {
        fprintf(stderr, "Unable close initialization database", zErrMsg);
        exit(1);
    }

    MemDB memdb = load_file(database_file);

    printf("have memdb of size %lld\n", memdb.size);
    char handle[100];
    snprintf(handle, sizeof(handle), "file:/tt?ptr=%p&sz=%lld", memdb.data, memdb.size);
    printf("Have handle %s\n", handle);

    extern int vfstrace_register(
        const char *zTraceName,
        const char *zOldVfsName,
        int (*xOut)(const char*,void*),
        void *pOutArg,
        int makeDefault
    );
    vfstrace_register("trace_unix", "unix", (int(*)(const char*,void*))fputs,stderr,1);
    vfstrace_register("trace_memvfs", "memvfs", (int(*)(const char*,void*))fputs,stderr,1);

    rc = sqlite3_open_v2(handle, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, "trace_memvfs");
    // rc = sqlite3_open_v2(database_file, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, "trace_unix");
    if (rc) {
        fprintf(stderr, "Unable to open database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return (1);
    }

    char* select = "SELECT COUNT(*) num_stock FROM stocks";
    sqlite3_exec(db, select, callback, 0, &zErrMsg);
    if (rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    rc = sqlite3_close(db);
    if (rc) {
        fprintf(stderr, "Unable close database", zErrMsg);
        exit(1);
    }

}
