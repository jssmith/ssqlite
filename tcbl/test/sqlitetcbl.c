#include <string.h>
#include <sys/param.h>
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

#include "../src/runtime.h"
#include "../src/tcbl_vfs.h"
#include "memvfs.h"
#include "unixvfs.h"


typedef struct appd {
    sqlite3_vfs *parent;
    vfs active_vfs;
    vfs base_vfs;
    vfs log_vfs;
    cvfs cache;
    bool has_txn;
} *appd;

typedef struct tcbl_sqlite3_fh {
    sqlite3_file base;
    appd ad;
    vfs_fh fh;
    bool has_txn;
    bool txn_active;
    bool is_log_file;
    uint64_t checkpoint_txn_ct;
} *tcbl_sqlite3_fh;

// TODO confirm that this should be static
#define ORIGVFS(p) (((appd)(p)->pAppData)->parent)

static bool trace_logging;
static bool info_logging;

#define TRACE(...) { if (trace_logging) { printf(__VA_ARGS__); printf("\n"); } }
#define INFO(...) { if (info_logging) { printf(__VA_ARGS__); printf("\n"); } }

// TODO list of tests to write
// - bounds check returned properly on file reads

#define SQLITE3_TCBL_PAGE_SIZE 4096

static inline int sqlite3_rc(int s)
{
    TRACE("result %s", s == TCBL_OK ? "ok" : s == TCBL_BOUNDS_CHECK ? "bounds" : "error");
    if (s != TCBL_OK && s != TCBL_BOUNDS_CHECK) {
        TRACE("error code %d\n", s);
    }
    return s == TCBL_OK ? SQLITE_OK : s == TCBL_BOUNDS_CHECK ? SQLITE_IOERR_SHORT_READ : SQLITE_ERROR;
}

static int tcblClose(sqlite3_file *pFile)
{
    tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
    TRACE("close %p\n", fh);
    return sqlite3_rc(vfs_close(fh->fh));
}

static int tcblRead(sqlite3_file *pFile,
                    void *zBuf,
                    int iAmt,
                    sqlite_int64 iOfst)
{
    TRACE("read %p %lld %d", pFile, iOfst, iAmt);
    tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
    return sqlite3_rc(vfs_read(fh->fh, zBuf, iOfst, iAmt));
}

static int tcblWrite(sqlite3_file *pFile,
                     const void *z,
                     int iAmt,
                     sqlite_int64 iOfst)
{
    TRACE("write %p %lld %d", pFile, iOfst, iAmt);
    tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
    fh->checkpoint_txn_ct += 1;
    return sqlite3_rc(vfs_write(fh->fh, z, iOfst, iAmt));
}

static int tcblTruncate(sqlite3_file *pFile,
                        sqlite_int64 size)
{
    TRACE("truncate %p %lld", pFile, size);
    tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
    fh->checkpoint_txn_ct += 1;
    return sqlite3_rc(vfs_truncate(fh->fh, size));
}

static int tcblSync(sqlite3_file *pFile, int flags)
{
    TRACE("sync %p", pFile);
    tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
    int rc = TCBL_OK;
    if (fh->has_txn) {
        if (fh->txn_active) {
            rc = vfs_txn_commit(fh->fh);
            fh->txn_active = false;
            fh->checkpoint_txn_ct += 1;
        }
        if (!rc) {
            if (fh->checkpoint_txn_ct > 20) {
                TRACE("checkpoint %p", pFile);
                rc = vfs_checkpoint(fh->fh);
                tcbl_stats_print(&((tcbl_fh) fh->fh)->stats);
                fh->checkpoint_txn_ct = 0;
            }
        }
    }
    return sqlite3_rc(rc);
}

static int tcblFileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
    TRACE("size %p", pFile);
    tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
    size_t file_size;
    int rc = sqlite3_rc(vfs_file_size(fh->fh, &file_size));
    *pSize = (sqlite_int64) file_size;
    return sqlite3_rc(rc);
}

static int tcblLock(sqlite3_file *pFile, int eFileLock)
{
    TRACE("lock %p", pFile);
    return SQLITE_OK;
}

static int tcblUnlock(sqlite3_file *pFile, int eFileLock) {
    TRACE("unlock %p", pFile);
    return SQLITE_OK;
}

static int tcblCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
    TRACE("check reserved lock %p", pFile);
    return SQLITE_OK;
}

static int tcblFileControl(sqlite3_file *pFile, int op, void *pArg)
{
    TRACE("file control %p %x", pFile, op);
    if (op == SQLITE_FCNTL_PRAGMA) {
        TRACE("fc = pragma %s\n", ((char **) pArg)[1]);
#ifdef TCBL_PERF_STATS
        if (strcmp("stats", ((char **) pArg)[1]) == 0) {
            tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
            tcbl_stats_print(&((tcbl_fh) fh->fh)->stats);
            return SQLITE_OK;
        }
#endif
        return SQLITE_NOTFOUND;
    }

    if (op == SQLITE_FCNTL_MMAP_SIZE) {
        *(uint64_t *) pArg = 0;
    }

    // TODO support SQLITE_IOCAP_BATCH_ATOMIC by wrapping with transaction

    if (op==SQLITE_FCNTL_VFSNAME) {
        *(char**)pArg = sqlite3_mprintf("tcbl");
    }

    return SQLITE_OK;
}

static int tcblSectorSize(sqlite3_file *pFile)
{
    TRACE("sector size %p", pFile);
    // TODO - maybe make this larger, understand what it is used for
    return SQLITE3_TCBL_PAGE_SIZE;
}

static int tcblDeviceCharacteristics(sqlite3_file *pFile){
    TRACE("device characteristics %p", pFile);
    // TODO is this right
    return SQLITE_IOCAP_POWERSAFE_OVERWRITE;
}

static int tcblShmMap(sqlite3_file *pFile, int iPg, int pgsz, int bExtend, void volatile  **pp)
{
    TRACE("shm map");
    return SQLITE_ERROR;
}


static int tcblShmLock(sqlite3_file *pFile, int offset, int n, int flags){
    TRACE("shm lock");
    return SQLITE_ERROR;
}

static void tcblShmBarrier(sqlite3_file *pFile){
    TRACE("shm barrier");
    /* noop */
}

static int tcblShmUnmap(sqlite3_file *pFile, int deleteFlag){
    TRACE("shm unmap");
    return SQLITE_OK;
}

static int tcblFetch(sqlite3_file *pFile,  sqlite3_int64 iOfst, int iAmt, void **pp)
{
    TRACE("fetch");
    return SQLITE_ERROR;
}

static int tcblUnfetch(sqlite3_file *pFile, sqlite3_int64 iOfst, void *pPage)
{
    TRACE("unfetch");
    return SQLITE_ERROR;
}

/* ************************************************* */

static sqlite3_io_methods tcbl_io_methods;

static bool has_ext(const char *name, const char *ext)
{
    size_t name_len = strlen(name);
    size_t ext_len = strlen(ext);
    return (name_len > ext_len && strcmp(&name[name_len - ext_len], ext) == 0);
}

static bool has_log_ext(const char* name)
{
    return has_ext(name, "-journal") || has_ext(name, "-wal");
}

static int tcblOpen(sqlite3_vfs *pVfs,
                    const char *zName,
                    sqlite3_file *pFile,
                    int flags,
                    int *pOutFlags)
{
    TRACE("open %s %x", zName, flags);
    tcbl_sqlite3_fh fh = (tcbl_sqlite3_fh) pFile;
    appd ad = pVfs->pAppData;
    fh->ad = ad;
    fh->base.pMethods = (const struct sqlite3_io_methods *) &tcbl_io_methods;
    bool is_log_file = has_log_ext(zName);
    int rc;
    if (is_log_file) {
        rc = vfs_open(ad->log_vfs, zName, &fh->fh);
    } else {
        rc = vfs_open(ad->active_vfs, zName, &fh->fh);
    }
    if (!rc) {
        if (pOutFlags) {
            *pOutFlags = flags;
        }
    }
    fh->has_txn = !is_log_file && ad->has_txn;
    fh->txn_active = false;
    fh->is_log_file = is_log_file;
    fh->checkpoint_txn_ct = 0;
    TRACE("have fh %p", fh->fh);
    return sqlite3_rc(rc);
}

static int tcblDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
    TRACE("delete %p", pVfs);
    appd ad = pVfs->pAppData;
    bool is_log_file = has_log_ext(zPath);
    if (is_log_file) {
        return sqlite3_rc(vfs_delete(ad->log_vfs, zPath));
    } else {
        return sqlite3_rc(vfs_delete(ad->active_vfs, zPath));
    }
}


static int tcblAccess(sqlite3_vfs *pVfs,
                      const char *zPath,
                      int flags,
                      int *pResOut)
{
    TRACE("access %s %d", zPath, flags);
    int rc = TCBL_OK;
    if (flags == SQLITE_ACCESS_EXISTS) {
        appd ad = pVfs->pAppData;
        int exists;
        bool is_log_file = has_log_ext(zPath);
        if (is_log_file) {
            rc = vfs_exists(ad->log_vfs, zPath, &exists);
        } else {
            rc = vfs_exists(ad->active_vfs, zPath, &exists);
        }
        *pResOut = exists ? 1 : 0;
    } else if (flags == SQLITE_ACCESS_READWRITE) {
        *pResOut = 1;
    } else {
        sqlite3_rc(1);
    }
    return sqlite3_rc(rc);
}

static int tcblFullPathname(sqlite3_vfs *pVfs,
                            const char *zPath,
                            int nOut,
                            char *zOut)
{
    TRACE("full pathname");
    sqlite3_snprintf(nOut, zOut, "%s", zPath);
    return SQLITE_OK;
}

static void *tcblDlOpen(sqlite3_vfs *pVfs, const char *zPath)
{
    TRACE("dl open");
    return ORIGVFS(pVfs)->xDlOpen(ORIGVFS(pVfs), zPath);
}

static void tcblDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg)
{
    TRACE("dl error");
    ORIGVFS(pVfs)->xDlError(ORIGVFS(pVfs), nByte, zErrMsg);
}

static void (*tcblDlSym(sqlite3_vfs *pVfs, void *p, const char *zSym))(void) {
    TRACE("dl sym");
    return ORIGVFS(pVfs)->xDlSym(ORIGVFS(pVfs), p, zSym);
}

static void tcblDlClose(sqlite3_vfs *pVfs, void *pHandle)
{
    TRACE("dl close");
    ORIGVFS(pVfs)->xDlClose(ORIGVFS(pVfs), pHandle);
}

static int tcblRandomness(sqlite3_vfs *pVfs, int nByte, char *zBufOut)
{
    TRACE("randomness");
    return ORIGVFS(pVfs)->xRandomness(ORIGVFS(pVfs), nByte, zBufOut);
}

static int tcblSleep(sqlite3_vfs *pVfs, int nMicro)
{
    TRACE("sleep");
    return ORIGVFS(pVfs)->xSleep(ORIGVFS(pVfs), nMicro);
}

static int tcblCurrentTime(sqlite3_vfs *pVfs, double *pTimeOut)
{
    TRACE("current time");
    return ORIGVFS(pVfs)->xCurrentTime(ORIGVFS(pVfs), pTimeOut);
}

static int tcblGetLastError(sqlite3_vfs *pVfs, int a, char *b)
{
    TRACE("get last error");
    return SQLITE_OK;
}

static int tcblCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *p)
{
    TRACE("current time int 64");
    return ORIGVFS(pVfs)->xCurrentTimeInt64(ORIGVFS(pVfs), p);
}


static sqlite3_vfs tcbl_sqlite3_vfs = {
        2,                                  /* iVersion */
        sizeof(struct tcbl_sqlite3_fh),     /* szOsFile */
        1024,                               /* mxPathname */
        0,                                  /* pNext */
        "tcbl",                             /* zName */
        0,                                  /* pAppData (set when registered) */
        tcblOpen,                           /* xOpen */
        tcblDelete,                         /* xDelete */
        tcblAccess,                         /* xAccess */
        tcblFullPathname,                   /* xFullPathname */
        tcblDlOpen,                         /* xDlOpen */
        tcblDlError,                        /* xDlError */
        tcblDlSym,                          /* xDlSym */
        tcblDlClose,                        /* xDlClose */
        tcblRandomness,                     /* xRandomness */
        tcblSleep,                          /* xSleep */
        tcblCurrentTime,                    /* xCurrentTime */
        tcblGetLastError,                   /* xGetLastError */
        tcblCurrentTimeInt64                /* xCurrentTimeInt64 */
};

static sqlite3_io_methods tcbl_io_methods = {
        3,                                      /* iVersion */
        tcblClose,                              /* xClose */
        tcblRead,                               /* xRead */
        tcblWrite,                              /* xWrite */
        tcblTruncate,                           /* xTruncate */
        tcblSync,                               /* xSync */
        tcblFileSize,                           /* xFileSize */
        tcblLock,                               /* xLock */
        tcblUnlock,                             /* xUnlock */
        tcblCheckReservedLock,                  /* xCheckReservedLock */
        tcblFileControl,                        /* xFileControl */
        tcblSectorSize,                         /* xSectorSize */
        tcblDeviceCharacteristics,              /* xDeviceCharacteristics */
        tcblShmMap,                             /* xShmMap */
        tcblShmLock,                            /* xShmLock */
        tcblShmBarrier,                         /* xShmBarrier */
        tcblShmUnmap,                           /* xShmUnmap */
        tcblFetch,                              /* xFetch */
        tcblUnfetch                             /* xUnfetch */
};

static int env_int(const char *var_name, int not_found, int exists_empty)
{
    char *x = getenv(var_name);
    if (!x) {
        return not_found;
    } else if (strlen(x) == 0) {
        return exists_empty;
    } else {
        return atoi(x);
    }
}

int load_test_db(vfs memvfs, const char *memvfs_fn, const char *local_fs_fn) {
    int rc;
    vfs_fh fh = NULL;
    FILE *f = NULL;
    size_t buff_size = 1024 * 1024;
    char buffer[buff_size];

    rc = vfs_open(memvfs, memvfs_fn, &fh);
    if (rc) {
        goto exit;
    }

    INFO("preloading %s to %s", local_fs_fn, memvfs_fn);
    f = fopen(local_fs_fn, "rb");
    if (!f) {
        rc = TCBL_IO_ERROR;
        goto exit;
    }
    fseek(f, 0, SEEK_END);
    size_t file_size = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    INFO("preload size is %ld", file_size);

    // Preallocate the space
    vfs_truncate(fh, file_size);

    size_t offs = 0;
    size_t remaining = file_size;
    while (remaining > 0) {
        size_t read_len = MIN(remaining, buff_size);
        size_t nread = fread(buffer, read_len, 1, f);
        if (nread != 1) {
            rc = TCBL_IO_ERROR;
            goto exit;
        }
        vfs_write(fh, buffer, offs, read_len);
        offs += read_len;
        remaining -= read_len;
    }

    exit:
    if (f) {
        fclose(f);
    }
    if (fh) {
        vfs_close(fh);
    }
    INFO("finished preloading - rc=%d", rc);
    return rc;
}

int sqlite3_sqlitetcbl_init(sqlite3 *db,
                     char **pzErrMsg,
                     const sqlite3_api_routines *pApi)
{
    SQLITE_EXTENSION_INIT2(pApi);
    int rc;
    appd ad = tcbl_malloc(0, sizeof(struct appd));
    ad->parent = sqlite3_vfs_find(0);

    int trace = env_int("TCBL_TRACE", 0, 1);
    trace_logging = trace != 0;
    info_logging = true;

    char* unix_mountpoint = getenv("TCBL_UNIX_MOUNTPOINT");
    char *preload_arg = getenv("TCBL_MEMVFS_PRELOAD");

    if (unix_mountpoint && preload_arg) {
        // conflicting environment variables
        rc = SQLITE_ERROR;
        goto exit;
    }

    char* base_vfs_name;
    if (unix_mountpoint) {
        if (unix_vfs_allocate(&ad->base_vfs, unix_mountpoint)) {
            rc = SQLITE_ERROR;
            goto exit;
        }
        base_vfs_name = "unix";
        rc = memvfs_allocate(&ad->log_vfs);
        if (rc) {
            rc = SQLITE_ERROR;
            goto exit;
        }
    } else {
        rc = memvfs_allocate(&ad->base_vfs);
        if (rc) {
            rc = SQLITE_ERROR;
            goto exit;
        }
        base_vfs_name = "memory";
        ad->log_vfs = ad->base_vfs;
    }

    int memvfs_only = env_int("TCBL_BASE_VFS_ONLY", 0, 1);
    if (!memvfs_only) {
        INFO("Activating tcbl with transactional layer");
        size_t cache_num_pages = (size_t) env_int("TCBL_CACHE_NUM_PAGES", 0, 0);
        if (cache_num_pages > 0) {
            size_t cache_page_size = (size_t) env_int("TCBL_CACHE_PAGE_SIZE", 1048576, 1048576);
            INFO("creating tcbl cache - page size: %lu, num pages: %lu", cache_page_size, cache_num_pages);
            rc = vfs_cache_allocate(&ad->cache, cache_page_size, cache_num_pages);
            if (rc) {
                rc = SQLITE_ERROR;
                goto exit;
            }
        } else {
            INFO("no tcbl cache");
            ad->cache = NULL;
        }
        ad->has_txn = true;
        rc = tcbl_allocate_2((tvfs*) &ad->active_vfs, ad->base_vfs, SQLITE3_TCBL_PAGE_SIZE, ad->cache);
        if (rc) {
            rc = SQLITE_ERROR;
            goto exit;
        }
    } else {
        INFO("Allocating tcbl with base vfs (%s) only", base_vfs_name);
        ad->has_txn = false;
        ad->active_vfs = ad->base_vfs;
    }

    // parse TCBL_PRELOAD, which has format host_file_path:tcbl_file_path
    if (preload_arg) {
        size_t len = strlen(preload_arg);
        char preload_arg_cpy[len + 1];
        memcpy(preload_arg_cpy, preload_arg, len + 1);
        char* local_filename = strtok(preload_arg_cpy, ":");
        char *vfs_filename = strtok(NULL, ":");
        if (!vfs_filename) {
            TRACE("invalid format in TCBL_MEMVFS_PRELOAD");
            rc = SQLITE_ERROR;
            goto exit;
        }
        printf("%s %s %s\n", local_filename, vfs_filename, preload_arg);

        rc = load_test_db(ad->base_vfs, vfs_filename, local_filename);
        if (rc) {
            rc = SQLITE_ERROR;
            goto exit;
        }
    }

    tcbl_sqlite3_vfs.pNext = sqlite3_vfs_find(0);
    tcbl_sqlite3_vfs.pAppData = ad;

    rc = sqlite3_vfs_register(&tcbl_sqlite3_vfs, 1);
    exit:
    if (rc == SQLITE_OK) {
        return SQLITE_OK_LOAD_PERMANENTLY;
    }
    return rc;
}
