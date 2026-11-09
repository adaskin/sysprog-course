/* kvpool_stub.c — a STAND-IN for your Lab 6 pool (given). It only exists so that this skeleton
 * builds. Copy YOUR kvpool.c and arena.c from Lab 6 into src/ and the Makefile will use them
 * instead of this file.
 *
 * Note: like your Lab 6 code, this pool is NOT thread-safe. Several planner threads will share it.
 */
#include "kvpool.h"
#include <stdlib.h>

KVPool *kv_pool_create(size_t block_size, size_t nblocks) {
    KVPool *p = calloc(1, sizeof(KVPool));
    if (!p) {
        return NULL;
    }
    p->block_size = (block_size + 15) & ~(size_t)15;
    p->nblocks = nblocks;
    p->nfree = nblocks;
    return p;
}

void kv_pool_destroy(KVPool *p) {
    free(p);
}

void *kv_block_alloc(KVPool *p) {
    if (p->nfree == 0) {
        return NULL;
    }
    p->nfree--;                                 /* unprotected: a data race if shared */
    return aligned_alloc(16, p->block_size);
}

void kv_block_free(KVPool *p, void *blk) {
    if (!blk) {
        return;
    }
    free(blk);
    p->nfree++;                                 /* unprotected: a data race if shared */
}

int kv_blocks_alloc(KVPool *p, size_t n, void *out[]) {
    if (n > p->nfree) {
        return -1;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = kv_block_alloc(p);
    }
    return 0;
}

void kv_blocks_free(KVPool *p, size_t n, void *blks[]) {
    for (size_t i = 0; i < n; i++) {
        kv_block_free(p, blks[i]);
    }
}

size_t kv_pool_nfree(const KVPool *p) {
    return p->nfree;
}

Arena *arena_create(size_t cap) {
    Arena *a = malloc(sizeof(Arena));
    if (!a) {
        return NULL;
    }
    a->buf = aligned_alloc(16, (cap + 15) & ~(size_t)15);
    a->used = 0;
    a->cap = cap;
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    size_t n = (size + 15) & ~(size_t)15;
    if (a->used + n > a->cap) {
        return NULL;
    }
    void *p = a->buf + a->used;
    a->used += n;
    return p;
}

void arena_reset(Arena *a) {
    a->used = 0;
}

void arena_destroy(Arena *a) {
    if (a) {
        free(a->buf);
        free(a);
    }
}
