#include "cycles.h"
#include "paired.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Plain vs. lock-prefixed increment, run cross-core as a ping-pong on a
 * single shared counter.
 *
 * Both lanes hammer the same counter. Each call batches REPS increments
 * so the per-call cost is comfortably above the harness floor; divide the
 * reported ticks by REPS for the per-increment cost.
 *
 * plain: separate load + store (mov; inc; mov). Whether the line is
 *   already held exclusively when the store runs determines whether the
 *   store is silent or needs a bus upgrade request; the preceding load
 *   can only ever obtain a shared or exclusive-but-unmodified copy, never
 *   ownership outright.
 * locked: single indivisible read-modify-write (lock incq). Requests
 *   ownership directly — one bus transaction instead of a read followed
 *   conditionally by an upgrade.
 *
 * Usage: ./pingpong_demo [cpu0 cpu1]
 * Defaults to cpu 0 and 1 if not given. Pick two logical CPUs that are
 * actually different physical cores (check `lscpu -e`) — two SMT
 * siblings of the same core share L1/L2 and the line never bounces, so
 * the comparison this demo exists to show collapses to noise.
 */

#define REPS 2000

static volatile uint64_t counter = 0;

static void op_plain_inc(void *arg)
{
    (void)arg;
    for (int i = 0; i < REPS; i++) {
        uint64_t v = counter;
        counter = v + 1;
    }
}

static void op_locked_inc(void *arg)
{
    (void)arg;
    for (int i = 0; i < REPS; i++)
        __asm__ volatile("lock incq %0" : "+m"(counter));
}

static void report_pair(const char *name, clockd_paired_result_t r)
{
    printf("%s\n", name);
    for (int lane = 0; lane < 2; lane++) {
        if (r.pin_result[lane] != CLOCKD_PIN_OK &&
            r.pin_result[lane] != CLOCKD_PIN_SKIPPED) {
            printf("  lane %d: pin failed (%s) — result below is NOT a\n"
                   "           pinned measurement, discard it\n",
                   lane, clockd_pin_strerror(r.pin_result[lane]));
        }
        printf("  lane %d  min %6lu  p50 %6lu  p99 %6lu  ticks/call"
               "  (%.2f ticks/op)\n",
               lane, r.stats[lane].min, r.stats[lane].p50,
               r.stats[lane].p99, (double)r.stats[lane].min / REPS);
    }
    printf("  sync floor: %lu ticks (handshake cost alone, same cores)\n\n",
           r.sync_floor);
}

int main(int argc, char **argv)
{
    int cpu0 = 0, cpu1 = 1;
    if (argc == 3) {
        cpu0 = atoi(argv[1]);
        cpu1 = atoi(argv[2]);
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [cpu0 cpu1]\n", argv[0]);
        return 1;
    }

    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu0 < 0 || cpu1 < 0 || cpu0 == cpu1 ||
        (ncpu > 0 && (cpu0 >= ncpu || cpu1 >= ncpu))) {
        fprintf(stderr, "invalid cpu pair (%d, %d) for a machine with "
                "%ld online cpus\n", cpu0, cpu1, ncpu);
        return 1;
    }

    clockd_init();
    printf("floor: %lu ticks (%.1f ns)\n\n",
           clockd_floor, clockd_ticks_to_ns(clockd_floor));
    printf("pinned to cpu %d and cpu %d — verify with `lscpu -e` that "
           "these are\ndistinct physical cores, not SMT siblings\n\n",
           cpu0, cpu1);

    clockd_pair_spec_t spec = {
        .fn  = op_plain_inc,
        .arg = { NULL, NULL },
        .cpu = { cpu0, cpu1 },
    };
    report_pair("plain increment (load; inc; store)",
                clockd_bench_paired(&spec));

    spec.fn = op_locked_inc;
    report_pair("lock-prefixed increment (lock incq)",
                clockd_bench_paired(&spec));

    return 0;
}