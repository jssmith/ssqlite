#include <sys/time.h>

typedef u64 ticks;

static ticks rdtsc(void)
{
    unsigned a, d;
    asm("cpuid");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));

    return (((ticks)a) | (((ticks)d) << 32));
}

static ticks ktime()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (tv.tv_sec << 32) | ((u64)tv.tv_usec * (1ull<<32)/1000000);
}

static void print_ticks(buffer b, ticks t) {
    print_u64(b, t>>32);
    u32 fraction  = t&((1ull<<32)-1);
    if (fraction) {
        push_char(b, '.');
        u32 digit;
        for (u32 log = (1ull<<32)/10; log && fraction; fraction -= digit * log, log /=10) {
            digit = fraction/log;
            push_char(b, '0' + digit);
        }
    }
}



static float ticks_to_float(buffer b, ticks t)
{
    float result = t>>32;
    u32 fraction  = t&((1ull<<32)-1);    
    for (u32 log = (1ull<<32)/10; log && fraction; fraction -= fraction/log, log /=10){
        result += fraction/log;
    }
    return result;
}

