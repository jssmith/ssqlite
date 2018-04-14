#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct buffer *buffer;

typedef struct status {
    int error;
    buffer description;
} *status;

#define check(__x) {status st = (__x); if (st) return st;}
#define true (1)
#define false (0)
typedef int boolean;

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;
typedef void *heap;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a):(b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a):(b))
#endif

#define allocate(__h, __b) (malloc(__b))
#define deallocate(__h, __x, __len) (free(__x))

static inline void panic (char *cause)
{
    eprintf ("nfs4 runtime error: %s\n", cause);
    abort();
}

#include <buffer.h>
#include <vector.h>
#include <stdarg.h>

static int parse_u64(buffer s, u64 *target)
{
    u64 result = 0;
    foreach_character (i, s) result = result * 10 + (i - '0');
    *target = result;
    return NFS4_OK;
}

// '4' prints as '004'?
static status print_u64(buffer d, u64 s)
{
    if (s == 0) {
        check_push_char(d, '0');
    } else {
        u32 log= 1;
        for (u64 b = s; b; b/= log, log*=10);
        for (u64 b = s; b; b -= b/log, log/=10)
            check_push_char(d, '0' + b/log);
    }
}


#include <ticks.h>
void vbprintf(buffer s, buffer fmt, va_list ap);
buffer aprintf(heap h, char *fmt, ...);
