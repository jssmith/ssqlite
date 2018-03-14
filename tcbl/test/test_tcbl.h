#ifndef TCBL_TEST_TCBL
#define TCBL_TEST_TCBL

#include "tcbl_vfs.h"

enum TestMode { MemVfs = 1, TcblVfs = 2 };

typedef struct test_env {
    enum TestMode test_mode;
    vfs test_vfs;
    vfs memvfs;
    int (*before_change)(vfs_fh);
    int (*after_change)(vfs_fh);
    int (*cleanup_change)(vfs_fh);
    int (*cleanup)(struct test_env *);
    vfs_fh fh[0];
} *test_env;

typedef struct change_fh {
    struct vfs_fh vfs_fh;
    vfs_fh orig_fh;
    test_env env;
    struct tvfs change_vfs;
} *change_fh;

void create_change_fh(test_env env, struct change_fh *fhout);

#endif // TCBL_TEST_TCBL