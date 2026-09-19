#include "cycles.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

uint64_t clockd_floor = 0;
static double ns_per_tick = 0.0;

uint64_t clockd_tsc(void)
{
    uint32_t lo, hi, aux;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux) :: "memory");
    return ((uint64_t)hi << 32) | lo;
}

/*
 * Measure TSC frequency by comparing TSC ticks against CLOCK_MONOTONIC
 * over a 100 ms busy-wait window. Returns ticks per second.
 */
static uint64_t tsc_frequency(void)
{
    struct timespec t0, t1;
    uint64_t c0, c1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    c0 = clockd_tsc();

    do {
        clock_gettime(CLOCK_MONOTONIC, &t1);
    } while ((t1.tv_sec  - t0.tv_sec)  * 1000000000LL +
             (t1.tv_nsec - t0.tv_nsec) < 100000000LL);   /* 100 ms */

    c1 = clockd_tsc();

    uint64_t elapsed_ns = (uint64_t)(t1.tv_sec  - t0.tv_sec)  * 1000000000ULL
                        + (uint64_t)(t1.tv_nsec - t0.tv_nsec);
    return (c1 - c0) * 1000000000ULL / elapsed_ns;
}

/*
 * Measure the minimum cost of two consecutive rdtscp reads.
 * This is the floor every measurement sits on top of.
 */
static uint64_t measure_floor(void)
{
    uint64_t min = UINT64_MAX;
    for (int i = 0; i < 500; i++) {
        uint64_t t0 = clockd_tsc();
        uint64_t t1 = clockd_tsc();
        uint64_t d  = t1 - t0;
        if (d < min) min = d;
    }
    return min;
}

void clockd_init(void)
{
    uint64_t freq = tsc_frequency();
    ns_per_tick   = 1e9 / (double)freq;
    clockd_floor = measure_floor();
}

uint64_t clockd_bench(void (*fn)(void *), void *arg)
{
    for (int i = 0; i < CLOCKD_WARMUP; i++)
        fn(arg);

    uint64_t min = UINT64_MAX;
    for (int r = 0; r < CLOCKD_RUNS; r++) {
        uint64_t t0 = clockd_tsc();
        fn(arg);
        uint64_t t1 = clockd_tsc();
        uint64_t d  = t1 - t0;
        if (d < min) min = d;
    }
    return min;
}

double clockd_ticks_to_ns(uint64_t ticks)
{
    return (double)ticks * ns_per_tick;
}

void clockd_report(const char *name, uint64_t ticks, size_t reps)
{
    double per_op_ticks = (double)ticks / (double)reps;
    double per_op_ns    = clockd_ticks_to_ns(ticks) / (double)reps;
    printf("%-32s  %7.2f ticks/op   %7.2f ns/op\n",
           name, per_op_ticks, per_op_ns);
}