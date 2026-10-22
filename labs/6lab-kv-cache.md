# Lab 6: The KV Cache Memory Pool

**BIL 322 System Programming — Week 6 Lab**
**Duration:** 1 week (due next Thursday's lab session)
**Submission:** GitHub Classroom (repository) + Google Classroom link

> **Revised handout.** The pool is now the plain `mymalloc` free list from
> lecture, with fixed-size blocks: each free block stores the address of the
> next free block, and the pool keeps a `free_head`. The index arrays and the
> required refcounting from the first version are gone (sharing is now a bonus).
> Use this `kvpool.h` and the skeleton below.

---

## 🎯 Context: From `mymalloc` to Real Workloads

In lecture we wrote `mymalloc` together: blocks, headers, boundary tags,
splitting, coalescing, first fit. That allocator works for *any* size —
and pays for that flexibility with searches, fragmentation, and fiddly cleanup.

This lab builds the other half of the story from Act 10: the same allocator
with **one fixed block size**, for a real AI-infrastructure workload — the
**KV cache** — plus a **request arena** for everything else a request owns.

From a systems point of view, a KV cache is just memory:

- large (gigabytes per batch),
- growing as the sequence grows,
- made of fixed-size blocks,
- freed all at once when the request ends.

When every block has the same size, `mymalloc` gets much simpler: no size
header, no splitting, no coalescing, no searching. A free block's bytes are
unused, so the free list lives *inside* the free blocks: each one stores the
address of the next free block. A request asks for `n` blocks, the pool pops
`n` blocks off the list; the request ends, the pool pushes them back.

**This lab is Part 1 of the end-of-semester project.** In Weeks 9–10 you will
make this pool thread-safe and put it behind a request queue; in Week 13+ it
may sit behind a socket server. Write it clean, test it well — you will reuse it.

---

## 📚 Learning Objectives

By the end of this lab you will be able to:

1. Build a self-contained allocator in a **single `mmap`'d region** using a
   flexible array member (`char data[]`).
2. Implement O(1) allocation/free with a **free list threaded through the
   free blocks** (`next` address stored in the block, plus a `free_head`).
3. Hand out **`n` fixed-size blocks** to a caller, all or nothing.
4. Implement a **bump-pointer arena** with 16-byte alignment.
5. Benchmark your allocator against libc `malloc` (time + peak RSS).
6. Keep a nontrivial C program Valgrind-clean.

## 🧰 Prerequisites

- Week 6 lecture (Acts 4, 6, 9, 10) and the in-class `mymalloc` code
  (especially the explicit free list).
- Week 2: pointer arithmetic, `offsetof`, alignment, flexible array members.
- `mmap`/`munmap` (Field Manual FM 3), Valgrind (FM 7).

---

## 🧠 The Design in One Picture

```
one mmap:
[ KVPool header ][ block 0 ][ block 1 ][ block 2 ][ block 3 ] ... [ block n-1 ]
  32 bytes        ^ p->data (16-byte aligned), every block is block_size bytes

after create, every block is free and linked in address order:

free_head ──► [block 0|next]──► [block 1|next]──► [block 2|next]──► ... ──► NULL

kv_blocks_alloc(p, 2, out):  out = {block 0, block 1}
free_head ──► [block 2|next]──► [block 3|next]──► ... ──► NULL

kv_blocks_free(p, 2, out):   push block 0, then block 1
free_head ──► [block 1|next]──► [block 0|next]──► [block 2|next]──► ... ──► NULL
```

- **Free block:** its first 8 bytes hold `next` (a `FreeBlock *`). The rest is unused.
- **Allocated block:** all `block_size` bytes belong to the caller. The pool does
  not track it at all; the caller may overwrite the old `next`, and that is fine,
  because `free` writes a fresh one.
- **Alloc** = pop from the head. **Free** = push onto the head. Both O(1).

---

## 📦 The Provided Header — `kvpool.h`

Do **not** change the struct layouts or prototypes. The grader compiles against this file.

```c
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
```

---

## 🛠️ Task 1: The KV Pool (55%)

Implement `kvpool.c`.

### Requirements

| # | Requirement |
| --- | --- |
| 1 | `kv_pool_create` uses **exactly one `mmap`** (`MAP_PRIVATE \| MAP_ANONYMOUS`) of `sizeof(KVPool) + nblocks * block_size` bytes. The header sits at the start; the blocks start at `p->data`. |
| 2 | **No `malloc`/`calloc`/`realloc`/`free` anywhere in `kvpool.c`.** The grader greps for them. |
| 3 | Initialize the free list in address order: block 0's `next` = block 1, …, block `nblocks-1`'s `next` = `NULL`; `free_head` = block 0; `nfree = nblocks`. |
| 4 | `kv_block_alloc`: O(1) pop from `free_head`; decrement `nfree`; return the block's address; return `NULL` when the pool is empty. |
| 5 | `kv_block_free`: **validate** first: the pointer is inside the block region and exactly on a block boundary; otherwise print to `stderr` and return without touching the list. Then push the block onto `free_head` (write its `next`) and increment `nfree`. `NULL` is silently ignored, like libc `free`. |
| 6 | `kv_blocks_alloc(pool, n, out)`: give the caller `n` blocks in `out[0..n-1]` and return `0`. **All or nothing:** if fewer than `n` blocks are free, return `-1` and take nothing (check `nfree` first). |
| 7 | `kv_blocks_free(pool, n, blks)`: return all `n` blocks (each one validated as in #5). |
| 8 | `kv_pool_destroy`: **exactly one `munmap`**, with the same length that `mmap` got. |
| 9 | Every returned block address must be **16-byte aligned**. |
| 10 | Edge cases: `nblocks == 0` or `block_size == 0` → return `NULL` from create. If `block_size` is not a multiple of 16, round it up (document this in your report). |

### Hints

- Write a small `pool_total(block_size, nblocks)` helper and use it in both
  `kv_pool_create` and `kv_pool_destroy`. `munmap` must get the same length
  that `mmap` got.
- Block `i` is at `p->data + i * p->block_size`. The index of a block is
  `((char *)blk - p->data) / p->block_size` — only after you checked it is in range.
- Why is `p->data` 16-byte aligned? `offsetof(KVPool, data)` is 32 on x86-64
  (four 8-byte fields), and `mmap` returns a page-aligned address.
  `block_size` is a multiple of 16, so every block stays aligned.
- Cast a block to `FreeBlock *` to read or write its `next`:
  `((FreeBlock *)blk)->next = p->free_head;`
- Implement the single-block functions first; `kv_blocks_alloc` and
  `kv_blocks_free` are short loops on top of them.
- Keep a debug function `kv_pool_dump()` (walks the list, counts the free
  blocks, compares with `nfree`) behind `#ifdef DEBUG` — it saves hours.

### Optional edge-case handling (not required)

These are not graded, but a robust allocator handles them. If you do, add a
test for each and mention it in `REPORT.md`.

| # | Edge case | What to do |
| --- | --- | --- |
| E1 | **Size overflow:** `nblocks * block_size` (or the total passed to `mmap`) wraps around `size_t`. | Detect it before computing the total and return `NULL` from create. |
| E2 | **Duplicate pointers:** `kv_blocks_free` gets the same block twice in one call. | See bonus B1. |
| E3 | **`n == 0`:** `kv_blocks_alloc(p, 0, out)`. | Return `0` and change nothing. |

---

## 🧩 Skeleton Code

Start from these files. Every `TODO` is yours; nothing else is required.

**`kvpool.c`**

```c
/* kvpool.c — BIL 322 Lab 6: fixed-size block pool with a free list */
#define _DEFAULT_SOURCE           /* MAP_ANONYMOUS */
#include "kvpool.h"
#include <stdio.h>
#include <sys/mman.h>

/* Bytes for the header plus all blocks. create and destroy both use it. */
static size_t pool_total(size_t block_size, size_t nblocks) {
    /* TODO */
    return 0;
}

/* Address of block i. */
static char *block_addr(KVPool *p, size_t i) {
    /* TODO: p->data + i * block_size */
    return NULL;
}

/* 1 if blk is the start of a block in this pool, else print a warning
 * (use `who` as the prefix) and return 0. */
static int block_valid(KVPool *p, void *blk, const char *who) {
    /* TODO: inside [p->data, p->data + nblocks * block_size) ?
     * TODO: on a block boundary? */
    return 0;
}

KVPool *kv_pool_create(size_t block_size, size_t nblocks) {
    /* TODO: reject 0 sizes; round block_size up to a multiple of 16 */
    /* TODO: one mmap of pool_total(...) bytes; MAP_FAILED -> NULL */
    /* TODO: fill block_size, nblocks, nfree */
    /* TODO: link block i -> block i+1, last block -> NULL */
    /* TODO: free_head = block 0 */
    return NULL;
}

void kv_pool_destroy(KVPool *p) {
    /* TODO: one munmap, same length as the mmap */
}

void *kv_block_alloc(KVPool *p) {
    /* TODO: empty -> NULL; else pop free_head, nfree-- */
    return NULL;
}

void kv_block_free(KVPool *p, void *blk) {
    /* TODO: NULL -> return; invalid -> return (block_valid warns) */
    /* TODO: push: blk->next = free_head; free_head = blk; nfree++ */
}

int kv_blocks_alloc(KVPool *p, size_t n, void *out[]) {
    /* TODO: n > nfree -> return -1 (take nothing) */
    /* TODO: out[i] = kv_block_alloc(p) for i < n */
    return -1;
}

void kv_blocks_free(KVPool *p, size_t n, void *blks[]) {
    /* TODO: kv_block_free each one */
}

size_t kv_pool_nfree(const KVPool *p) {
    /* TODO */
    return 0;
}
```

**`arena.c`**

```c
/* arena.c — BIL 322 Lab 6: bump-pointer request arena */
#include "kvpool.h"
#include <stdlib.h>

Arena *arena_create(size_t cap) {
    /* TODO: allocate the Arena and a 16-byte aligned buffer of cap bytes
     * (malloc + aligned_alloc, or mmap: your choice, explain it in REPORT.md) */
    return NULL;
}

void *arena_alloc(Arena *a, size_t size) {
    /* TODO: round size up to 16; NULL if it does not fit; else bump `used` */
    return NULL;
}

void arena_reset(Arena *a) {
    /* TODO: O(1) */
}

void arena_destroy(Arena *a) {
    /* TODO: release the buffer and the Arena */
}
```

**`test_kvpool.c`** (starter: add the remaining tests from Task 3a)

```c
#include "kvpool.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static void test_exhaustion(void) {
    KVPool *p = kv_pool_create(64, 4);
    assert(p != NULL && kv_pool_nfree(p) == 4);
    void *b[4];
    assert(kv_blocks_alloc(p, 4, b) == 0);
    assert(kv_block_alloc(p) == NULL);         /* empty */
    kv_blocks_free(p, 4, b);
    assert(kv_pool_nfree(p) == 4);
    kv_pool_destroy(p);
    printf("exhaustion: ok\n");
}

/* TODO: reuse, all-or-nothing, invalid free, arena alignment,
 *       arena reset, round trip */

int main(void) {
    test_exhaustion();
    return 0;
}
```

**`Makefile`**

```make
CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g

all: test_kvpool

test_kvpool: test_kvpool.c kvpool.c arena.c kvpool.h
	$(CC) $(CFLAGS) -o $@ test_kvpool.c kvpool.c arena.c

bench: bench.c kvpool.c arena.c kvpool.h
	$(CC) $(CFLAGS) -O2 -o $@ bench.c kvpool.c arena.c

clean:
	rm -f test_kvpool bench

.PHONY: all clean
```

---

## 🏟️ Task 2: The Request Arena (20%)

Implement `arena.c` (the arena *may* use `malloc` or `mmap` — your choice;
say which and why in your report).

| # | Requirement |
| --- | --- |
| 1 | `arena_alloc` rounds the requested size up to a multiple of 16 and bumps the pointer. Return `NULL` when the arena is full (no growth required). |
| 2 | Every returned pointer is 16-byte aligned — write an assertion in your tests. |
| 3 | `arena_reset` sets `used = 0` — O(1), no frees. |
| 4 | `arena_destroy` releases the backing memory; nothing leaks. |

Then simulate one request in your test program:

```c
Arena *a = arena_create(4096);
char *prompt   = arena_alloc(a, 120);   /* prompt copy    */
char *tokens   = arena_alloc(a, 256);   /* token buffer   */
char *response = arena_alloc(a, 1024);  /* output buffer  */
void *kv[8];
kv_blocks_alloc(pool, 8, kv);           /* the request's KV blocks */
/* ... request runs ... */
kv_blocks_free(pool, 8, kv);            /* blocks go back to the pool */
arena_destroy(a);                       /* one call. done. */
```

---

## 📊 Task 3: Tests, Benchmark, Report (25%)

### 3a. Test suite — `test_kvpool.c`

Must cover, at minimum:

1. **Exhaustion:** allocate all blocks → next alloc returns `NULL`.
2. **Reuse (LIFO):** free a block → next alloc returns the same address.
3. **All or nothing:** with `k` blocks free, `kv_blocks_alloc(p, k + 1, out)` returns `-1` and `kv_pool_nfree` is still `k`; `kv_blocks_alloc(p, k, out)` succeeds and returns `k` **distinct**, 16-byte aligned addresses. Write `block_size` bytes into each one: no block may overlap another.
4. **Invalid free:** free a pointer from outside the pool and a pointer into the middle of a block → warning printed, `kv_pool_nfree` unchanged, pool still works.
5. **Arena alignment:** every `arena_alloc` result satisfies `((uintptr_t)p % 16) == 0`.
6. **Arena reset:** after reset, the next alloc returns the arena's start address.
7. **Round trip:** 10 000 alloc/free cycles of `n` blocks → pool ends with all blocks free.

### 3b. Benchmark — `bench.c`

Simulate a batch workload:

- `R = 1000` requests, block size `16384`, pool with `512` blocks.
- Each request: create a 4 KB arena, take 8–32 KV blocks (pick `n` randomly)
  with one `kv_blocks_alloc` call, write one byte into each block, then free everything.

Measure both:

| Variant | How |
| --- | --- |
| **Your pool + arena** | as above |
| **libc baseline** | same workload using `malloc(block_size)`/`free` for each block |

Report: wall time (`clock_gettime(CLOCK_MONOTONIC)`) and peak RSS
(`getrusage(..., &ru)` → `ru_maxrss`). Present as a small table.

### 3c. Report — `REPORT.md`

Answer briefly (2–5 sentences each):

1. Why does the pool version fragment less than the `malloc` version?
2. Where does the free list's `next` pointer live, and why does it cost zero extra memory? What happens to it when a caller writes into a block it got from the pool?
3. Where did the time go in each variant — allocation, freeing, or the workload itself? How do you know?
4. One design decision you made (block size, validation behavior, arena backing) and its tradeoff.
5. **Project connection:** what breaks if two threads call `kv_blocks_alloc` at once, and what is the minimal fix? (You don't implement it yet — Weeks 9–10.)

---

## 🚀 Bonus (up to +10%)

| Bonus | Description |
| --- | --- |
| **B1** | Double-free detection: freeing a block that is already on the free list prints a warning and does nothing. (Walking the list is O(nfree); say in the report why that is acceptable only in a debug build, or find a cheaper way.) |
| **B2** | Shared prefix blocks: a `uint16_t` refcount per block, so two requests can hold the same block (`kv_block_share` = refcount++, free = refcount-- and push only at 0), plus `kv_block_clone` (if shared, take a fresh block, `memcpy`, drop one reference, return the copy). The refcounts cannot live inside the blocks (allocated blocks belong to the caller), so put them in the same `mmap`, after the last block. Test the diverging-prefix scenario. |
| **B3** | Thread-safety teaser: wrap the alloc/free functions with a single `pthread_mutex_t` and stress-test with 4 threads. (We cover mutexes properly in Week 9.) |

For B2, put the extra prototypes in a separate `kvpool_share.h` so the graded
`kvpool.h` stays unchanged, and put its tests in `test_bonus.c`.
Note that the pool cannot detect writes: sharing is a contract, and a writer
must call `kv_block_clone` *before* modifying a shared block (see the lecture).

---

## ✅ Assessment

| Component | Weight |
| --- | --- |
| Task 1: KV pool correctness + single-mmap rule | 55% |
| Task 2: arena | 20% |
| Task 3: tests + benchmark + report | 25% |
| Bonus | up to +10% |

**Grader's checklist:**

```bash
make
./test_kvpool                                   # all tests pass
valgrind --leak-check=full --error-exitcode=1 ./test_kvpool   # clean
grep -nE '\b(malloc|calloc|realloc|free)\(' kvpool.c         # no libc heap in the pool
make bench && ./bench                           # table prints, no crashes
```

(`\b` means `kv_block_free(` does not count as a libc `free(`.)

Zero-tolerance items: memory leaks in Valgrind, corrupted free list after an
invalid free, missing report.

---

## 📬 Submission

Push to your GitHub Classroom repository:

```
kvpool.h        (given — unchanged)
kvpool.c
arena.c
test_kvpool.c
bench.c
Makefile
REPORT.md
kvpool_share.h  (optional, only with B2)
test_bonus.c    (optional, only with B2)
```

Then paste the repository link into Google Classroom.

**AI policy reminder (from the syllabus):** you may use AI tools for
explanation and debugging, but you must be able to explain *every line* you
submit — especially the free-list pointer casts. List any AI assistance in
`REPORT.md`.

---

## ❓ FAQ

**Q: A KV cache grows as the sequence grows. Why does the pool use fixed-size blocks?**
Because that is how real KV caches are managed. A sequence does not resize one
big buffer; it takes one more fixed-size block whenever the last one fills up.
Every block has the same size, so any free block can serve any request, and
external fragmentation disappears. Allocation and freeing are per-block O(1)
operations. The price is internal fragmentation: the last block of a sequence
is usually only partly used. Keep this trade-off in mind for the report.

**Q: How is this different from the explicit free list in lecture?**
It is the same list. In `mymalloc` each block also needs a size header, and
`malloc` must search for a block that is big enough, then split it; `free`
must coalesce neighbors. Here every block has the same size, so the size lives
once in the pool header, the first free block always fits, and there is
nothing to split or merge.

**Q: Where is the header of an allocated block?**
There is none. The pool does not need to know anything about an allocated
block: its size is `block_size`, and its address tells you its index. That is
why the caller gets all `block_size` bytes.

**Q: Why must `block_size` be at least 16?**
A free block must hold an 8-byte `next` pointer, and blocks must stay
16-byte aligned. Rounding up to a multiple of 16 covers both.

**Q: Why does `kv_blocks_alloc` check `nfree` first instead of popping until it fails?**
All or nothing. If it popped 5 of 8 blocks and then failed, it would have to
push the 5 back. With `nfree` the answer is known in O(1) before touching the list.

**Q: Can I use `sbrk` instead of `mmap`?**
No — the lab requires `mmap`. `mmap` regions can be returned cleanly with
`munmap`, and `sbrk` behaves badly once threads arrive (Week 7).

**Q: What should `kv_block_free` do with `NULL`?**
Same convention as libc `free`: silently ignore.

**Q: Does the arena need to grow when full?**
No. Return `NULL`. (Growth is a bonus if you want it — document the strategy.)

---

## 🔭 Where This Goes Next

| Week | What we add to this code |
| --- | --- |
| 9 | `pthread_mutex_t` around the pool (Week 9: synchronization) |
| 10 | A thread pool pulling requests off a queue, each with its arena and its KV blocks (Week 10: producer/consumer) |
| 12 | Optional: shared-memory variant of the pool between processes (`mmap` + `MAP_SHARED`) |
| 13+ | Final project: a multithreaded, inference-like request server — this pool is its memory subsystem |

Keep a clean, tested copy of this lab. It is not throwaway code.
