#define _GNU_SOURCE   /* pthread_setaffinity_np is a GNU extension */

#include "paired.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

clockd_pin_result_t clockd_pin_thread(int cpu)
{
    if (cpu < 0)
        return CLOCKD_PIN_SKIPPED;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0)
        return CLOCKD_PIN_ESET;

    return CLOCKD_PIN_OK;
}

const char *clockd_pin_strerror(clockd_pin_result_t r)
{
    switch (r) {
    case CLOCKD_PIN_OK:      return "pinned";
    case CLOCKD_PIN_EINVAL:  return "invalid cpu index";
    case CLOCKD_PIN_ESET:    return "pthread_setaffinity_np failed";
    case CLOCKD_PIN_SKIPPED: return "no pin requested (cpu < 0)";
    }
    return "unknown";
}

/* ------------------------------------------------------------------ */
/* core two-lane handshake + timing loop, shared by the real benchmark */
/* and the sync-floor measurement below                                */

typedef struct {
    int    lane;
    void (*fn)(void *);
    void  *arg;
    int    cpu;
    _Atomic int *ready;   /* ready[0], ready[1] — this lane's own slot    */
    _Atomic int *go;      /* shared release flag                         */
    uint64_t *samples;    /* CLOCKD_RUNS-sized, owned by the caller       */
    clockd_pin_result_t pin_result;
} lane_ctx_t;

static void *lane_main(void *p)
{
    lane_ctx_t *w = (lane_ctx_t *)p;

    w->pin_result = clockd_pin_thread(w->cpu);

    atomic_store_explicit(&w->ready[w->lane], 1, memory_order_release);
    while (!atomic_load_explicit(w->go, memory_order_acquire))
        ; /* busy-wait: a blocking wait here would add a syscall to the
             exact handoff this harness exists to measure */

    for (int i = 0; i < CLOCKD_WARMUP; i++)
        w->fn(w->arg);

    for (int r = 0; r < CLOCKD_RUNS; r++) {
        uint64_t t0 = clockd_tsc();
        w->fn(w->arg);
        uint64_t t1 = clockd_tsc();
        w->samples[r] = t1 - t0;
    }
    return NULL;
}

/*
 * Runs fn(arg[lane]) on both lanes as one paired unit and fills
 * samples[lane][0..CLOCKD_RUNS). The main thread is the referee: it
 * waits for both lanes to report ready, then releases both together so
 * neither lane's timed loop gets a head start from the other.
 */
static void paired_core(void (*fn)(void *), void *arg[2], const int cpu[2],
                         clockd_pin_result_t pin_result[2],
                         uint64_t samples[2][CLOCKD_RUNS])
{
    _Atomic int ready[2];
    _Atomic int go;
    atomic_init(&ready[0], 0);
    atomic_init(&ready[1], 0);
    atomic_init(&go, 0);

    lane_ctx_t ctx[2];
    pthread_t th[2];

    for (int lane = 0; lane < 2; lane++) {
        ctx[lane].lane    = lane;
        ctx[lane].fn      = fn;
        ctx[lane].arg     = arg[lane];
        ctx[lane].cpu     = cpu[lane];
        ctx[lane].ready   = ready;
        ctx[lane].go      = &go;
        ctx[lane].samples = samples[lane];
        pthread_create(&th[lane], NULL, lane_main, &ctx[lane]);
    }

    while (!(atomic_load_explicit(&ready[0], memory_order_acquire) &&
             atomic_load_explicit(&ready[1], memory_order_acquire)))
        ;
    atomic_store_explicit(&go, 1, memory_order_release);

    for (int lane = 0; lane < 2; lane++)
        pthread_join(th[lane], NULL);

    for (int lane = 0; lane < 2; lane++)
        pin_result[lane] = ctx[lane].pin_result;
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static clockd_stats_t stats_from_samples(uint64_t samples[CLOCKD_RUNS])
{
    uint64_t sorted[CLOCKD_RUNS];
    memcpy(sorted, samples, sizeof(sorted));
    qsort(sorted, CLOCKD_RUNS, sizeof(sorted[0]), cmp_u64);

    uint64_t sum = 0;
    for (int r = 0; r < CLOCKD_RUNS; r++)
        sum += sorted[r];

    clockd_stats_t st;
    st.min  = sorted[0];
    st.p50  = sorted[CLOCKD_RUNS / 2];
    st.mean = (double)sum / (double)CLOCKD_RUNS;
    st.p90  = sorted[(CLOCKD_RUNS * 90) / 100];
    st.p99  = sorted[(CLOCKD_RUNS * 99) / 100];
    st.max  = sorted[CLOCKD_RUNS - 1];
    return st;
}

static void noop(void *arg)
{
    (void)arg;
}

clockd_paired_result_t clockd_bench_paired(const clockd_pair_spec_t *spec)
{
    clockd_paired_result_t result;
    uint64_t samples[2][CLOCKD_RUNS];

    void *arg[2]  = { spec->arg[0], spec->arg[1] };
    int   cpu[2]  = { spec->cpu[0], spec->cpu[1] };

    paired_core(spec->fn, arg, cpu, result.pin_result, samples);
    for (int lane = 0; lane < 2; lane++)
        result.stats[lane] = stats_from_samples(samples[lane]);

    /*
     * Sync floor: identical handshake, identical core placement, fn
     * swapped for a no-op. This is the cost the handshake itself adds —
     * measured, not assumed — so it can be read alongside the real
     * result instead of being invisibly baked into it.
     */
    uint64_t floor_samples[2][CLOCKD_RUNS];
    clockd_pin_result_t floor_pin[2];
    void *floor_arg[2] = { NULL, NULL };
    paired_core(noop, floor_arg, cpu, floor_pin, floor_samples);

    uint64_t floor_min = floor_samples[0][0];
    for (int lane = 0; lane < 2; lane++)
        for (int r = 0; r < CLOCKD_RUNS; r++)
            if (floor_samples[lane][r] < floor_min)
                floor_min = floor_samples[lane][r];
    result.sync_floor = floor_min;

    return result;
}