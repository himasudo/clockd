# clockd

TSC-based microbenchmark harness for Linux x86-64.

Uses `rdtscp` to measure wall-clock time in CPU ticks with low overhead.
Calibrates tick frequency against `CLOCK_MONOTONIC` at startup.

I built this because I kept reading cycle counts in blog posts and had no way
to verify them on my own machine. Now I can.

---

## What it is

A small C library (`lib/cycles.h` / `lib/cycles.c`) that provides:

- TSC frequency calibration
- Harness floor measurement (the cost of the timing machinery itself)
- A benchmark runner with warmup and minimum-across-runs reporting
- Tick-to-nanosecond conversion

The floor is reported first in every example. Every result should be
interpreted relative to it. A measurement smaller than the floor is noise.

---

## Requirements

- Linux x86-64
- GCC
- `constant_tsc` and `nonstop_tsc` CPU flags (verify: `grep -m1 tsc /proc/cpuinfo`)

---

## Build

```sh
make
```

Builds two examples at `-O2`:

- `cycles_demo`: harness floor, integer add, integer divide, 4-variable chain
- `cache_sweep`: a Sattolo-shuffled pointer-chase working-set-size sweep,
  4 KB to 128 MB, reported with `clockd_bench_stats`

---

## Usage

```c
#include "cycles.h"

#define REPS 1000

static volatile uint64_t sink;

static void op_add(void *arg) {
    (void)arg;
    uint64_t x = 1;
    for (int i = 0; i < REPS; i++)
        __asm__ volatile("addq $1, %0" : "+r"(x));
    sink = x;
}

int main(void) {
    clockd_init();

    printf("floor: %lu ticks  (%.1f ns)\n",
           clockd_floor,
           clockd_ticks_to_ns(clockd_floor));

    uint64_t t = clockd_bench(op_add, NULL);
    clockd_report("integer add x1000", t, REPS);
}
```

`clockd_bench` runs `fn(arg)` with warmup and returns the minimum tick
count across `CLOCKD_RUNS` timed runs. Divide by `reps` (the number of
operations `fn` performs per call) for per-operation cost.

### When one number isn't enough

`clockd_bench` reports the minimum — the right answer when `fn` does the
same work every call. Some access patterns don't: a working-set sweep near
a cache boundary mixes hits and misses from one call to the next, and the
minimum alone hides that mix. `clockd_bench_stats` runs the same warmup and
`CLOCKD_RUNS` timed calls, but keeps every sample instead of only the
minimum, and returns `min`, `p50`, `mean`, `p90`, `p99`, `max`:

```c
clockd_stats_t s = clockd_bench_stats(chase, &ctx);
printf("min %.2f  p50 %.2f  p99 %.2f  max %.2f (ns/hop)\n",
       clockd_ticks_to_ns(s.min) / hops,
       clockd_ticks_to_ns(s.p50) / hops,
       clockd_ticks_to_ns(s.p99) / hops,
       clockd_ticks_to_ns(s.max) / hops);
```

`mean` is a `double`, already averaged, so convert it with
`s.mean * clockd_ticks_to_ns(1)` rather than passing it to
`clockd_ticks_to_ns` (which takes a tick count, not a tick average).

Use `clockd_bench` for a simple latency number. Use `clockd_bench_stats`
when the thing being measured can cost different amounts from one call to
the next and the shape of that spread is part of the answer.

### Pointer chasing

`lib/pointer_chase.h` / `.c` builds a Sattolo-shuffled chain of nodes — a
dependent, randomly ordered walk that visits every node exactly once before
repeating, which defeats both the hardware prefetcher and out-of-order
overlap. It is a dependency of `cache_sweep` and reusable for any benchmark
that needs genuine miss latency rather than a sequential-access number:

```c
#include "pointer_chase.h"

void *buf;
struct pc_node *head = pointer_chase_build(count, 64, &buf);  /* 64B nodes */
/* ... clockd_bench(_stats) a function that calls pointer_chase_walk ... */
free(buf);
```

---

## Design notes

**Why batch operations inside fn?**
`rdtscp` costs roughly 40-50 ticks. A single integer add costs ~1 tick.
Timing one add per call would report floor, not the add. Batch 1000 ops
per call so the floor is ~5% of the measurement rather than 5000%.

**Why minimum and not mean?**
Noise in CPU measurements is one-directional: interrupts, migrations, and
cache evictions only add time, never subtract it. The minimum is the closest
estimate of the true cost. Mean and median are useful for diagnosing variance
but not for the cost itself.

**Why `rdtscp` and not `rdtsc`?**
`rdtsc` does not wait for preceding instructions to retire. Out-of-order
execution can move it before or after the work under test. `rdtscp` waits
for retirement before sampling. This reduces boundary smearing at the cost
of a few extra ticks.

**Why does `clockd_bench_stats` exist alongside `clockd_bench`, not instead
of it?**
Storing `CLOCKD_RUNS` samples and sorting them costs more memory and time
than tracking a running minimum. Most benchmarks here don't need that: a
tight add or divide loop costs the same every call, and the minimum is the
least-noise estimate of that one true cost (see above). The stats version
earns its cost only when a call's cost can genuinely vary, such as a
pointer chase whose hit/miss mix depends on whether the working set fits
in cache. Forcing every caller to pay for percentiles they don't need would
make the common case slower for no benefit.

---

## License

MIT. See [LICENSE](LICENSE).