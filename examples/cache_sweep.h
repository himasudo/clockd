#include "cycles.h"
#include "pointer_chase.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN_BYTES  (4ULL * 1024)
#ifndef MAX_BYTES
#define MAX_BYTES  (128ULL * 1024 * 1024)
#endif
#define MIN_HOPS   1000000ULL

struct chase_ctx {
    struct pc_node *cursor;
    uint64_t hops;
};

static void chase(void *arg)
{
    struct chase_ctx *ctx = arg;
    ctx->cursor = pointer_chase_walk(ctx->cursor, ctx->hops);
}

int main(void)
{
    clockd_init();
    printf("floor: %" PRIu64 " ticks  (%.1f ns)\n\n",
           clockd_floor, clockd_ticks_to_ns(clockd_floor));

    double ns_per_tick = clockd_ticks_to_ns(1);

    printf("%10s %10s %10s %10s %10s %10s %10s\n",
           "size", "min", "p50", "mean", "p90", "p99", "max");
    printf("%10s %10s %10s %10s %10s %10s %10s\n",
           "", "ns/hop", "ns/hop", "ns/hop", "ns/hop", "ns/hop", "ns/hop");

    for (uint64_t bytes = MIN_BYTES; bytes <= MAX_BYTES; bytes *= 2) {
        size_t count = (size_t)(bytes / 64);
        void *buf;
        struct pc_node *head = pointer_chase_build(count, 64, &buf);
        if (!head) {
            fprintf(stderr, "cannot allocate %" PRIu64 " bytes\n", bytes);
            return EXIT_FAILURE;
        }

        /* One untimed lap: warms every page in the chain before timing. */
        pointer_chase_walk(head, count);

        struct chase_ctx ctx = {
            .cursor = head,
            .hops = count > MIN_HOPS ? count : MIN_HOPS
        };
        clockd_stats_t s = clockd_bench_stats(chase, &ctx);
        double h = (double)ctx.hops;

        if (bytes < 1024 * 1024)
            printf("%8" PRIu64 "KB", bytes / 1024);
        else
            printf("%8" PRIu64 "MB", bytes / (1024 * 1024));

        printf(" %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f\n",
               clockd_ticks_to_ns(s.min) / h,
               clockd_ticks_to_ns(s.p50) / h,
               (s.mean * ns_per_tick) / h,
               clockd_ticks_to_ns(s.p90) / h,
               clockd_ticks_to_ns(s.p99) / h,
               clockd_ticks_to_ns(s.max) / h);

        free(buf);
    }

    return EXIT_SUCCESS;
}