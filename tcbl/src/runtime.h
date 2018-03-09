#ifndef TCBL_RUNTIME_H
#define TCBL_RUNTIME_H

#include <stdlib.h>
#include <stdio.h>

//#define MEMORY_TESTING

#ifndef MEMORY_TESTING
#define tcbl_malloc(__h, __b) (malloc(__b))
#define tcbl_free(__h, __x, __len) (free(__x))
#else
extern void* _test_malloc(const size_t size, const char* file, const int line);
extern void _test_free(void* const ptr, const char* file, const int line);
#define tcbl_malloc(__h, __b) (_test_malloc(__b, __FILE__, __LINE__))
#define tcbl_free(__h, __x, __len) (_test_free(__x, __FILE__, __LINE__))
#endif

#endif //TCBL_RUNTIME_H
