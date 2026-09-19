#include "cycles.h"

#include <stdint.h>
#include <stdio.h>

#define REPS 1000

/*
 * sink prevents the compiler from discarding results that are
 * computed but never used.
 */
static volatile uint64_t sink;

/* ------------------------------------------------------------------ */
/* floor — no work; measures harness overhead alone                    */

static void op_noop(void *arg)
{
    (void)arg;
}

/* ------------------------------------------------------------------ */
/* integer add — one dependent add per iteration                       */
/*                                                                     */
/* volatile runtime seed prevents the compiler from folding the entire */
/* loop to a constant. Inline asm on x prevents dead-store removal.   */

static volatile uint64_t add_seed = 1;

static void op_add(void *arg)
{
    (void)arg;
    uint64_t x = add_seed;
    for (int i = 0; i < REPS; i++)
        __asm__ volatile("addq $1, %0" : "+r"(x));
    sink = x;
}

/* ------------------------------------------------------------------ */
/* integer divide — one dependent idivq per iteration                  */
/*                                                                     */
/* Divisor is volatile so the compiler cannot see its value at compile */
/* time and replace the divide with a multiply-shift sequence.         */

static volatile uint64_t div_divisor = 7;

static void op_divide(void *arg)
{
    (void)arg;
    uint64_t x = 1000000ULL;
    uint64_t d = div_divisor;
    for (int i = 0; i < REPS; i++) {
        x = x / d;
        if (x == 0) x = 1000000ULL;   /* prevent underflow without a branch */
    }
    sink = x;
}

/* ------------------------------------------------------------------ */
/* 4-variable add chain — four partially independent streams           */
/*                                                                     */
/* a, b, c, d each depend only on their own previous value, so the    */
/* core can advance all four in parallel when execution ports allow.   */

static volatile uint64_t chain_seed = 1;

static void op_chain(void *arg)
{
    (void)arg;
    uint64_t s = chain_seed;
    uint64_t a = s, b = s + 1, c = s + 2, d = s + 3;
    for (int i = 0; i < REPS; i++) {
        __asm__ volatile("addq $1, %0" : "+r"(a));
        __asm__ volatile("addq $1, %0" : "+r"(b));
        __asm__ volatile("addq $1, %0" : "+r"(c));
        __asm__ volatile("addq $1, %0" : "+r"(d));
    }
    sink = a ^ b ^ c ^ d;
}

/* ------------------------------------------------------------------ */

int main(void)
{
    clockd_init();

    printf("floor            : %lu ticks  (%.1f ns)\n\n",
           clockd_floor,
           clockd_ticks_to_ns(clockd_floor));

    uint64_t t;

    t = clockd_bench(op_noop, NULL);
    clockd_report("noop (harness overhead)", t, 1);

    t = clockd_bench(op_add, NULL);
    clockd_report("integer add x1000", t, REPS);

    t = clockd_bench(op_divide, NULL);
    clockd_report("integer divide x1000", t, REPS);

    t = clockd_bench(op_chain, NULL);
    clockd_report("4-var add chain x1000", t, REPS);

    return 0;
}