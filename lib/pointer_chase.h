#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * One node of a pointer-chase chain. Real nodes are allocated wider than
 * this (see pointer_chase_build) so each one fills a full cache line;
 * `next` is always the first field, at offset 0.
 */
struct pc_node {
    struct pc_node *next;
};

/*
 * pointer_chase_build — allocate `count` nodes and link them into one
 * Sattolo-shuffled cycle (C3: a dependent, randomly ordered chain visiting
 * every node exactly once before repeating).
 *
 * Each node is padded to `node_size` bytes, rounded up to a 64-byte
 * multiple (minimum 64), and the whole buffer is 64-byte aligned.
 * Returns the head node (node 0 in the buffer) or NULL on allocation
 * failure. *out_buf receives the raw buffer pointer — keep it, since
 * nodes are visited out of allocation order and the head pointer alone
 * is not enough to free() the buffer later.
 */
struct pc_node *pointer_chase_build(size_t count, size_t node_size, void **out_buf);

/*
 * pointer_chase_walk — follow `next` pointers `hops` times from `head`.
 * Returns the final node reached, so the caller can make the result
 * escape (same dead-code-elimination concern as a sink variable).
 */
struct pc_node *pointer_chase_walk(struct pc_node *head, uint64_t hops);