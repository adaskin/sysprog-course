/* kvpool.h — BIL 322 Lab 6. Given; do not modify the struct layouts. */
#ifndef KVPOOL_H
#define KVPOOL_H

#include <stddef.h>
#include <stdint.h>

/* A free block stores the address of the next free block in its own
 * first 8 bytes. An allocated block belongs entirely to the caller.   */
typedef struct FreeBlock {
    struct FreeBlock *next;
} FreeBlock;

/* One mmap'd region holds EVERYTHING:
 *
 *  [ KVPool header (32 B) ][ block 0 ][ block 1 ] ... [ block nblocks-1 ]
 *                           ^ p->data, 16-byte aligned, each block_size bytes
 */
typedef struct {
    size_t     block_size;  /* bytes per block (multiple of 16)            */
    size_t     nblocks;     /* number of blocks in the pool                */
    size_t     nfree;       /* how many blocks are on the free list now    */
    FreeBlock *free_head;   /* first free block; NULL = pool exhausted     */
    char       data[];      /* the blocks — flexible array member, last    */
} KVPool;

KVPool *kv_pool_create(size_t block_size, size_t nblocks);
void    kv_pool_destroy(KVPool *pool);                   /* exactly one munmap */

void   *kv_block_alloc(KVPool *pool);                    /* pop one; NULL when empty   */
void    kv_block_free(KVPool *pool, void *blk);          /* push one; bad ptr: warn, ignore */

int     kv_blocks_alloc(KVPool *pool, size_t n, void *out[]);  /* n blocks or none: 0 / -1 */
void    kv_blocks_free(KVPool *pool, size_t n, void *blks[]);  /* give n blocks back       */

size_t  kv_pool_nfree(const KVPool *pool);

/* Request arena — variable-size metadata with a shared lifetime */
typedef struct { char *buf; size_t used; size_t cap; } Arena;

Arena *arena_create(size_t cap);
void  *arena_alloc(Arena *a, size_t size);          /* 16-byte aligned; NULL if full */
void   arena_reset(Arena *a);                       /* O(1): forget everything */
void   arena_destroy(Arena *a);

#endif /* KVPOOL_H */
