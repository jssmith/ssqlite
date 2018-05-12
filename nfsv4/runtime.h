#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct buffer *buffer;

typedef unsigned long long bytes;

typedef struct status {
    int error;
    buffer description;
} *status;

typedef struct heap {
    void *(*alloc)(struct heap *h, bytes b);
    void (*dealloc)(struct heap *h, void *, bytes b);
    void (*destroy)(struct heap *h);
    bytes pagesize;
    bytes allocated;
} *heap;

#define check(__x) {status st = (__x); if (st) return st;}
#define true (1)
#define false (0)
typedef int boolean;

typedef unsigned char u8;
typedef char s8;
typedef unsigned int u32;
typedef unsigned long u64;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a):(b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a):(b))
#endif

#define allocate(__h, __b) (__h->alloc(__h, __b))
#define deallocate(__h, __x, __len) (__h->dealloc(__h, __x, __len))
#define destroy(__h) ((__h->destroy)(__h))

#include <buffer.h>
#include <vector.h>
#include <stdarg.h>

static int parse_u64(buffer s, u64 *target)
{
    u64 result = 0;
    foreach_character (i, s) result = result * 10 + (i - '0');
    *target = result;
    return STATUS_OK;
}

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

extern heap mallocheap;

#include <ticks.h>
void vbprintf(buffer s, buffer fmt, va_list ap);
buffer aprintf(heap h, char *fmt, ...);


#ifndef eprintf
// stupid zero args thing
#define eprintf(format, ...) {                          \
        buffer b = aprintf(mallocheap, format, __VA_ARGS__);     \
        write(2, b->contents, buffer_length(b));        \
    }
#endif


void bprintf(buffer b, char *fmt, ...);
void format_number(buffer s, u64 x, int base, int pad);
heap init_heap();
#include <freelist.h>
