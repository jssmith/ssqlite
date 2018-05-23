#ifndef TCBL_TEST_RUNTIME_H
#define TCBL_TEST_RUNTIME_H

#include "tcbl_runtime.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmocka.h"
#include <stdint.h>

#define TCBL_TEST_PAGE_SIZE 64
#define TCBL_TEST_CACHE_PAGE_SIZE 256
#define TCBL_TEST_MAX_FH 2

#define RC_OK(__x) assert_int_equal(__x, TCBL_OK)
#define RC_OK_E(__x) { rc = __x; if (rc != TCBL_OK) goto exit; }
#define RC_OK_R(__x) { rc = __x; if (rc != TCBL_OK) { printf("have rc %d\n", rc); raise(SIGINT); goto exit; } }
#define RC_NOT_OK(__x) assert_int_not_equal(__x, TCBL_OK)
#define RC_EQ(__x, __erc) assert_int_equal(__x, __erc)

void prep_data(void* data, size_t data_len, uint64_t seed);

#endif //TCBL_TEST_RUNTIME_H
