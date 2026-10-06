#pragma once

#include <stdint.h>
#include "cycles.h"

/*
 * Two-thread pinned benchmarking.
 *
 * Deliberately scoped to exactly two threads/lanes. Generalizing to N
 * threads needs a different handshake (this one is a two-flag rendezvous,
 * not a counting barrier) and is left for later; this covers the
 * cache-line-ownership-handoff case, which is inherently pairwise.
 *
 * No correctness layer here either: this does not detect a run whose
 * threads interleaved incorrectly (e.g. both landed on the same core
 * despite distinct cpu requests, or a scheduler preemption stretched one
 * sample far past the rest). That is future scope, not silently assumed
 * away — callers should look at both clockd_pin_result_t values and treat
 * a lane's max/p99 blowing out relative to its own min as a sign a sample
 * was preempted, not as paired-handoff cost.
 */

typedef enum {
    CLOCKD_PIN_OK = 0,     /* pinned successfully                        */
    CLOCKD_PIN_EINVAL,     /* cpu index was negative                     */
    CLOCKD_PIN_ESET,       /* pthread_setaffinity_np failed (see errno)  */
    CLOCKD_PIN_SKIPPED,    /* cpu < 0 passed on purpose: no pin requested */
} clockd_pin_result_t;

/*
 * clockd_pin_thread — pin the calling thread to cpu.
 * Returns the outcome rather than failing silently: a benchmark that
 * believes it is pinned but isn't is measuring the scheduler, not
 * coherence traffic, and will look clean while being wrong.
 */
clockd_pin_result_t clockd_pin_thread(int cpu);

/* Human-readable reason, for reporting a non-OK result. */
const char *clockd_pin_strerror(clockd_pin_result_t r);

/*
 * clockd_pair_spec_t — one function run on two lanes, each with its own
 * argument and (optionally) its own CPU. cpu[i] < 0 means "don't pin lane
 * i" rather than being treated as an error.
 */
typedef struct {
    void (*fn)(void *arg);
    void *arg[2];
    int   cpu[2];
} clockd_pair_spec_t;

typedef struct {
    clockd_stats_t      stats[2];       /* per-lane timing distribution   */
    clockd_pin_result_t pin_result[2];  /* per-lane pinning outcome       */
    uint64_t             sync_floor;    /* cost of the handshake alone,
                                           measured on the same two cpus
                                           with fn replaced by a no-op —
                                           subtract this before reading
                                           stats as "handoff cost"        */
} clockd_paired_result_t;

/*
 * clockd_bench_paired — time fn(arg[0]) on lane 0 and fn(arg[1]) on lane 1
 * as one paired unit, not as two independent single-thread timings.
 *
 * Both lanes spin on a shared ready/go handshake (busy-wait, not a futex:
 * a futex syscall would itself add latency to exactly the handoff this is
 * meant to measure) so their timed loops start together rather than
 * however far apart pthread_create happened to land them. Each lane then
 * runs its own CLOCKD_WARMUP + CLOCKD_RUNS loop and records its own
 * samples, which clockd_bench_stats-style percentiles are computed from
 * independently — a paired result is two distributions, not one.
 *
 * sync_floor is measured with the identical handshake and pin placement
 * but fn swapped for a no-op, so the handshake's own cost is reported
 * rather than silently absorbed into what looks like coherence cost.
 */
clockd_paired_result_t clockd_bench_paired(const clockd_pair_spec_t *spec);