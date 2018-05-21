#include <string.h>
#include <stdio.h>
#include "perfstats.h"

static void timespec_elapsed(struct timespec begin,
                             struct timespec end,
                             struct timespec *diff)
{
    if (end.tv_nsec <= begin.tv_nsec) {
        diff->tv_sec = end.tv_sec - begin.tv_sec;
        diff->tv_nsec = end.tv_nsec - begin.tv_nsec;
    } else {
        diff->tv_sec = end.tv_sec - begin.tv_sec - 1;
        diff->tv_nsec = 1000000000 - end.tv_nsec + begin.tv_nsec;
    }
}

static void timespec_add(struct timespec a,
                         struct timespec b,
                         struct timespec *result)
{
    if (a.tv_nsec + b.tv_nsec > 1000000000) {
        result->tv_nsec = a.tv_nsec + b.tv_nsec - 1000000000;
        result->tv_sec = a.tv_sec + b.tv_sec - 1;
    } else {
        result->tv_nsec = a.tv_nsec + b.tv_nsec;
        result->tv_sec = a.tv_sec + b.tv_sec;
    }
}


void tcbl_stats_init(tcbl_stats s)
{
    memset(&s->counters, 0, sizeof(*s));
}

void tcbl_stats_timer_begin(struct timespec *begin_timespec)
{
    clock_gettime(CLOCK_MONOTONIC, begin_timespec);
}

void tcbl_stats_timer_end(tcbl_stats s, enum tcbl_timer id, struct timespec begin_time)
{
    if (s) {
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        struct timespec elapsed_time;
        timespec_elapsed(begin_time, end_time, &elapsed_time);
        timespec_add(elapsed_time, s->timers[id].elapsed, &s->timers[id].elapsed);
        s->timers[id].ct += 1;
    }
}

void tcbl_stats_counter_inc(tcbl_stats s, enum tcbl_counter id)
{
    if (s) {
        s->counters[id] += 1;
    }
}

char* tcbl_stats_counter_name(enum tcbl_counter id)
{
    switch (id) {
        case TCBL_COUNTER_TXN_BEGIN:
            return "txn_begin";
        case TCBL_COUNTER_TXN_COMMIT:
            return "txn_commit";
        case TCBL_COUNTER_TXN_ABORT:
            return "txn_abort";
        case TCBL_COUNTER_CHECKPOINT:
            return "checkpoint";
        case TCBL_COUNTER_READ:
            return "read";
        case TCBL_COUNTER_WRITE:
            return "write";
        case TCBL_COUNTER_CACHE_HIT:
            return "cache_hit";
        case TCBL_COUNTER_CACHE_MISS:
            return "cache_miss";
        case TCBL_COUNTER_CACHE_EVICT:
            return "cache_evict";
        case TCBL_BC_LOG_READ:
            return "bc_log_read";
        case TCBL_COUNTER_END:
            return "unknown";
    }
    return NULL;
}

char *tcbl_stats_timer_name(enum tcbl_timer id)
{
    switch (id) {
        case TCBL_TIMER_BLOCK_FETCH:
            return "block_fetch";
        case TCBL_TIMER_END:
            return "unknown";
    }
    return NULL;
}

void tcbl_stats_print(tcbl_stats s)
{
    printf("========================================\n");
    for (int i = 0; i < TCBL_COUNTER_END; i++) {
        printf("%s: %ld\n", tcbl_stats_counter_name(i), s->counters[i]);
    }
    for (int i = 0; i < TCBL_TIMER_END; i++) {
        struct tcbl_stat_timer t = s->timers[i];
        printf("%s: %ld %ld\n", tcbl_stats_timer_name(i), t.ct,
               t.elapsed.tv_sec * 1000000000 + t.elapsed.tv_nsec);
    }
    printf("========================================\n");
}
