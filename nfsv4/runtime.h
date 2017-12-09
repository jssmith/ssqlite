#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define true (1)
#define false (0)
typedef int boolean;

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;
typedef u32 bytes;
typedef void *heap;

typedef void *heap;
#define allocate(__h, __b) (malloc(__b))
#define deallocate(__h, __x, __len) (free(__x))


#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a):(b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a):(b))
#endif


static inline void panic (char *cause)
{
    printf ("nfs4 runtime error: %s\n", cause);
    abort();
}

