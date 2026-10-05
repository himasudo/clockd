#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * Harness tuning. Can be overridden before including this header.
 *   CLOCKD_WARMUP  — iterations before timing begins
 *   CLOCKD_RUNS    — timed runs; minimum is reported
 */
#ifndef CLOCKD_WARMUP
#define CLOCKD_WARMUP 32
#endif

#ifndef CLOCKD_RUNS
#define CLOCKD_RUNS 200
#endif

/*
 * Minimum cost of two consecutive rdtscp reads with no work between them.
 * Set by clockd_init(). Subtract from a raw batch measurement to remove
 * harness overhead. Report it first so any result can be interpreted
 * relative to it.
 */
extern uint64_t clockd_floor;

/*
 * clockd_init — calibrate TSC frequency and measure harness floor.
 * Must be called once before any other clockd_* function.
 */
void clockd_init(void);

/*
 * clockd_tsc — read TSC via rdtscp.
 * rdtscp waits for all preceding instructions to retire before sampling,
 * which reduces out-of-order smearing at measurement boundaries.
 */
uint64_t clockd_tsc(void);

/*
 * clockd_bench — time one call to fn(arg).
 * Warms up, then runs fn(arg) CLOCKD_RUNS times and returns the minimum
 * tick count observed. fn is expected to perform the operations under test,
 * batched internally if the per-operation cost is smaller than the floor.
 */
uint64_t clockd_bench(void (*fn)(void *), void *arg);

/*
 * clockd_stats_t — a distribution across CLOCKD_RUNS timed calls to fn(arg).
 * All fields are tick counts except mean, which is a tick count averaged as
 * a double (convert with clockd_ticks_to_ns(1) * mean, since
 * clockd_ticks_to_ns takes a uint64_t).
 */
typedef struct {
    uint64_t min;
    uint64_t p50;
    double   mean;
    uint64_t p90;
    uint64_t p99;
    uint64_t max;
} clockd_stats_t;

/*
 * clockd_bench_stats — time CLOCKD_RUNS calls to fn(arg), keep every sample.
 * Same warmup and run-count behavior as clockd_bench, but stores all
 * CLOCKD_RUNS tick counts instead of only tracking the minimum, then sorts
 * them once and reports percentiles. This costs CLOCKD_RUNS * 8 bytes of
 * stack/heap and a sort that clockd_bench does not pay.
 * Use clockd_bench for simple latency comparisons; use this when the
 * access pattern can be a mix of distinct costs (e.g. a working-set sweep
 * that straddles a cache boundary) and the minimum alone would hide it.
 */
clockd_stats_t clockd_bench_stats(void (*fn)(void *), void *arg);

/*
 * clockd_ticks_to_ns — convert a tick count to nanoseconds.
 * Requires clockd_init() to have been called.
 */
double clockd_ticks_to_ns(uint64_t ticks);

/*
 * clockd_report — print one result line to stdout.
 *   name  — label for the measurement
 *   ticks — return value of clockd_bench
 *   reps  — number of operations fn performed per call
 *
 * Prints: name  N.NN ticks/op  N.NN ns/op
 */
void clockd_report(const char *name, uint64_t ticks, size_t reps);