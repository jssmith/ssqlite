#ifndef TCBL_PERFSTATS_H
#define TCBL_PERFSTATS_H

#include <time.h>
#include <stdint.h>

enum tcbl_counter {
    TCBL_COUNTER_TXN_BEGIN,
    TCBL_COUNTER_TXN_COMMIT,
    TCBL_COUNTER_TXN_ABORT,
    TCBL_COUNTER_CHECKPOINT,
    TCBL_COUNTER_READ,
    TCBL_COUNTER_WRITE,
    TCBL_COUNTER_CACHE_HIT,
    TCBL_COUNTER_CACHE_MISS,
    TCBL_COUNTER_CACHE_EVICT,
    TCBL_BC_LOG_READ,
    TCBL_COUNTER_END
};

enum tcbl_timer {
    TCBL_TIMER_BLOCK_FETCH,
    TCBL_TIMER_END
};

struct tcbl_stat_timer {
    uint64_t ct;
    struct timespec elapsed;
};

typedef struct tcbl_stats {
    uint64_t counters[TCBL_COUNTER_END];
    struct tcbl_stat_timer timers[TCBL_TIMER_END];
} *tcbl_stats;

void tcbl_stats_init(tcbl_stats s);
void tcbl_stats_timer_begin(struct timespec *begin_time);
void tcbl_stats_timer_end(tcbl_stats, enum tcbl_timer, struct timespec begin_time);
void tcbl_stats_counter_inc(tcbl_stats, enum tcbl_counter);

char* tcbl_stats_counter_name(enum tcbl_counter id);

void tcbl_stats_print(tcbl_stats);

#endif //TCBL_PERFSTATS_H
