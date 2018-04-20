#ifndef TCBL_RUNTIME_H
#define TCBL_RUNTIME_H

#include <stdlib.h>
#include <stdio.h>

#define TCBL_OK                 0x00
#define TCBL_NOT_IMPLEMENTED    0x01
#define TCBL_ALLOC_FAILURE      0x02
#define TCBL_BAD_ARGUMENT       0x03
#define TCBL_BOUNDS_CHECK       0x04
#define TCBL_TXN_ACTIVE         0x05
#define TCBL_NO_TXN_ACTIVE      0x06
#define TCBL_INVALID_LOG        0x07
#define TCBL_CONFLICT_ABORT     0x08
#define TCBL_LOG_NOT_FOUND      0x09
#define TCBL_SNAPSHOT_EXPIRED   0x0a
#define TCBL_FILE_NOT_FOUND     0x0b
#define TCBL_IO_ERROR           0x0c
#define TCBL_INTERNAL_ERROR     0x0d

//#define TCBL_MEMVFS_VERBOSE

//#define TCBL_MEMORY_TESTING

#ifndef TCBL_MEMORY_TESTING
#define tcbl_malloc(__h, __b) (malloc(__b))
#define tcbl_free(__h, __x, __len) ((void)(__len),free(__x))
#else
extern void* _test_malloc(const size_t size, const char* file, const int line);
extern void _test_free(void* const ptr, const char* file, const int line);
#define tcbl_malloc(__h, __b) (_test_malloc(__b, __FILE__, __LINE__))
#define tcbl_free(__h, __x, __len) (_test_free(__x, __FILE__, __LINE__))
#endif

#endif //TCBL_RUNTIME_H
