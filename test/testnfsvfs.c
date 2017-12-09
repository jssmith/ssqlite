#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sqlite3.h"

/*
 * Demonstration of read, update, and create functionality for the
 * nfs vfs.
 */

void usage()
{
    fprintf(stderr, "Usage testnfsvfs [ read | create | update ] database_file\n");
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    int i;
    for(i=0; i<argc; i++){
        printf(">> %s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

void load_nfs_ext()
{
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    rc = sqlite3_open(":memory:", &db);
    if( rc ){
        fprintf(stderr, "Unable to open initialization database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    rc = sqlite3_enable_load_extension(db, 1);
    if( rc ){
        fprintf(stderr, "Unable to enable extension loading");
        exit(1);
    }

    rc = sqlite3_load_extension(db, "../nfsv4/nfs4", 0, &zErrMsg);
    if( rc ){
        fprintf(stderr, "Unable to load extension %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(1);
    }

    rc = sqlite3_close(db);
    if( rc ){
        fprintf(stderr, "Unable close initialization database", zErrMsg);
        exit(1);
    }
}

void trace_nfs_ext()
{
    extern int vfstrace_register(
        const char *zTraceName,
        const char *zOldVfsName,
        int (*xOut)(const char*,void*),
        void *pOutArg,
        int makeDefault
    );
    vfstrace_register("trace_nfs4", "nfs4", (int(*)(const char*,void*))fputs,stderr,1);
}

void read_test(const char* database_file)
{
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;
    rc = sqlite3_open_v2(database_file, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, "trace_nfs4");

    if( rc ){
        fprintf(stderr, "Unable to open database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    char* select = "SELECT COUNT(*) num_stock FROM stocks";
    sqlite3_exec(db, select, callback, 0, &zErrMsg);
    if( rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    rc = sqlite3_close(db);
    if( rc ){
        fprintf(stderr, "Unable close database", zErrMsg);
        exit(1);
    }
}

void create_test(const char* database_file ){
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;
    rc = sqlite3_open_v2(database_file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, "trace_nfs4");

    if( rc ){
        fprintf(stderr, "Unable to open database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    char* create_sql = "CREATE TABLE stocks (date text, trans text, symbol text, qty real, price real)";
    sqlite3_exec(db, create_sql, callback, 0, &zErrMsg);
    if( rc!=SQLITE_OK){
        fprintf(stderr, "SQL error in create: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(1);
    }

    char* insert_sql = "INSERT INTO stocks VALUES ('2006-01-05','BUY','RHAT',100,35.14)";
    sqlite3_exec(db, insert_sql, callback, 0, &zErrMsg);
    if( rc!=SQLITE_OK){
        fprintf(stderr, "SQL error in insert: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(1);
    }

    rc = sqlite3_close(db);
    if( rc ){
        fprintf(stderr, "Unable close database", zErrMsg);
        exit(1);
    }
}

void update_test(const char* database_file)
{
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    srandom(time(NULL));

    rc = sqlite3_open_v2(database_file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, "trace_nfs4");

    if( rc ){
        fprintf(stderr, "Unable to open database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    char* select = "SELECT COUNT(*) num_stock FROM stocks";
    rc = sqlite3_exec(db, select, callback, 0, &zErrMsg);
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(1);
    }

    sqlite3_stmt *stmt;
    char* update_sql = "INSERT INTO stocks VALUES (?,'BUY','RHAT',?,35.14)";
    rc = sqlite3_prepare_v2(db, update_sql, -1, &stmt, NULL);
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error on prepare: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_bind_int64(stmt, 1, (u_int64_t)time(NULL));
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error on bind time: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_bind_double(stmt, 2, random()%200+1);
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error on bind qty: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_step(stmt);
    if( rc!=SQLITE_DONE ){
        fprintf(stderr, "SQL error on step: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_finalize(stmt);
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error on finalize: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_close(db);
    if( rc ){
        fprintf(stderr, "Unable close database", zErrMsg);
        exit(1);
    }
}

int main(int argc, char** argv)
{
    if( argc != 3 ){
        usage();
        exit(1);
    }
    char* operation = argv[1];
    char* database_file = argv[2];

    void (*test)(const char*);
    if( strcmp("read",operation)==0 ){
    test = read_test;
    }else if( strcmp("create",operation)==0 ){
        test = create_test;
    }else if( strcmp("update",operation)==0 ){
        test = update_test;
    }else{
        usage();
        exit(1);
    }

    load_nfs_ext();
    trace_nfs_ext();

    test(database_file);
    return 0;
}
