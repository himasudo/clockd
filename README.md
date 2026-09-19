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

Builds one example at `-O2`:

- `cycles_demo`: harness floor, integer add, integer divide, 4-variable chain

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

---

## Used by

[perio](https://github.com/himasudo/perio), an async I/O library built on io_uring.

---

## License

MIT. See [LICENSE](LICENSE).