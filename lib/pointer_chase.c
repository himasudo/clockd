#include "pointer_chase.h"

#include <stdlib.h>

/* Deterministic PRNG so a chain's shuffle is reproducible run to run. */
static uint64_t pc_rng_state = UINT64_C(0xD1CE5EEDC0FFEE11);

static uint64_t pc_rand_u64(void)
{
    uint64_t x = pc_rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    pc_rng_state = x;
    return x;
}

/* Uniformly choose 0 <= result < bound without modulo bias. */
static size_t pc_rand_below(size_t bound)
{
    uint64_t threshold = (uint64_t)(-(uint64_t)bound) % bound;
    uint64_t r;
    do {
        r = pc_rand_u64();
    } while (r < threshold);
    return (size_t)(r % bound);
}

static inline struct pc_node *node_at(unsigned char *buf, size_t i, size_t stride)
{
    return (struct pc_node *)(buf + i * stride);
}

struct pc_node *pointer_chase_build(size_t count, size_t node_size, void **out_buf)
{
    size_t stride = node_size < sizeof(struct pc_node) ? sizeof(struct pc_node) : node_size;
    stride = (stride + 63) & ~(size_t)63;   /* round up to one cache line */

    unsigned char *buf = aligned_alloc(64, count * stride);
    if (!buf) {
        *out_buf = NULL;
        return NULL;
    }

    size_t *perm = malloc(count * sizeof(*perm));
    if (!perm) {
        free(buf);
        *out_buf = NULL;
        return NULL;
    }
    for (size_t i = 0; i < count; ++i)
        perm[i] = i;

    /* Sattolo's shuffle: j drawn from [0, i-1] guarantees one N-cycle,
     * never a short sub-cycle (C3). */
    for (size_t i = count - 1; i > 0; --i) {
        size_t j = pc_rand_below(i);
        size_t tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }

    for (size_t i = 0; i < count; ++i)
        node_at(buf, i, stride)->next = node_at(buf, perm[i], stride);

    free(perm);
    *out_buf = buf;
    return node_at(buf, 0, stride);
}

struct pc_node *pointer_chase_walk(struct pc_node *head, uint64_t hops)
{
    struct pc_node *p = head;
    for (uint64_t i = 0; i < hops; ++i)
        p = p->next;
    return p;
}