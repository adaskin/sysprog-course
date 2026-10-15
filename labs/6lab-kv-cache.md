# Lab 6: The KV Cache Memory Pool

**BIL 322 System Programming — Week 6 Lab**
**Duration:** 1 week (due next Thursday's lab session)
**Submission:** GitHub Classroom (repository) + Google Classroom link

---

## 🎯 Context: From `mymalloc` to Real Workloads

In lecture we wrote `mymalloc` together: blocks, headers, boundary tags,
splitting, coalescing, first fit. That allocator works for *any* size —
and pays for that flexibility with searches, fragmentation, and fiddly cleanup.

This lab builds the other half of the story from Act 10: a **fixed-size block
pool** for a real AI-infrastructure workload — the **KV cache** — plus a
**request arena** for everything else a request owns.

From a systems point of view, a KV cache is just memory:

- large (gigabytes per batch),
- growing as the sequence grows,
- made of fixed-size blocks,
- sometimes shared between requests (common prompt prefixes),
- freed all at once when the request ends.

**This lab is Part 1 of the end-of-semester project.** In Weeks 9–10 you will
make this pool thread-safe and put it behind a request queue; in Week 13+ it
may sit behind a socket server. Write it clean, test it well — you will reuse it.

---

## 📚 Learning Objectives

By the end of this lab you will be able to:

1. Build a self-contained allocator in a **single `mmap`'d region** using a
   flexible array member (`char data[]`).
2. Implement O(1) allocation/free with an **index-based free list**.
3. Implement **reference-counted sharing** of blocks (application-level COW).
4. Implement a **bump-pointer arena** with 16-byte alignment.
5. Benchmark your allocator against libc `malloc` (time + peak RSS).
6. Keep a nontrivial C program Valgrind-clean.

## 🧰 Prerequisites

- Week 6 lecture (Acts 4, 6, 9, 10) and the in-class `mymalloc` code.
- Week 2: pointer arithmetic, `offsetof`, alignment, flexible array members.
- `mmap`/`munmap` (Field Manual FM 3), Valgrind (FM 7).

---

## 📦 The Provided Header — `kvpool.h`

Do **not** change the struct layouts. The grader compiles against this file.

```c
/* kvpool.h — BIL 322 Lab 6. Given; do not modify struct layouts. */
#ifndef KVPOOL_H
#define KVPOOL_H

#include <stddef.h>
#include <stdint.h>

#define KVPOOL_EMPTY UINT32_MAX

/* One mmap'd region holds EVERYTHING:
 *
 *  [ KVPool header ][ next_free[] ][ refcount[] ][ pad ][ block 0 ][ block 1 ]...
 *                    nblocks×4B     nblocks×2B    ↑     each block_size bytes
 *                                              rounded so block 0
 *                                              is 16-byte aligned
 */
typedef struct {
    size_t   block_size;   /* payload bytes per block (multiple of 16)      */
    size_t   nblocks;      /* number of blocks in the pool                  */
    uint32_t free_head;    /* index of first free block; KVPOOL_EMPTY=none  */
    char     data[];       /* flexible array member — do NOT replace        */
} KVPool;

KVPool *kv_pool_create(size_t block_size, size_t nblocks);
void   *kv_block_alloc(KVPool *pool);               /* NULL when exhausted   */
void    kv_block_free(KVPool *pool, void *ptr);     /* invalid ptr: warn, ignore */
void    kv_block_share(KVPool *pool, void *ptr);    /* refcount++            */
void    kv_pool_destroy(KVPool *pool);              /* exactly one munmap    */

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
| 1 | `kv_pool_create` uses **exactly one `mmap`** (`MAP_PRIVATE \| MAP_ANONYMOUS`). Compute the total size so the header, both metadata arrays, alignment padding, and all blocks fit. |
| 2 | **No `malloc`/`calloc`/`realloc`/`free` anywhere in `kvpool.c`.** The grader greps for them. |
| 3 | Initialize the free list as a chain `0 → 1 → … → nblocks-1 → KVPOOL_EMPTY`, and all refcounts to 0. |
| 4 | `kv_block_alloc`: O(1) pop from the free list; set refcount to 1; return the block's address; return `NULL` when exhausted. |
| 5 | `kv_block_free`: convert pointer → index. **Validate**: pointer inside the block region and exactly on a block boundary; otherwise print to `stderr` and return without corrupting anything. If `refcount > 1`, just decrement. Otherwise reset refcount and push onto the free list. |
| 6 | `kv_block_share`: increment the block's refcount (validate as above). |
| 7 | `kv_pool_destroy`: **exactly one `munmap`**, with the exact total size recomputed from the layout (hint: `(pool_blocks(p) - (char *)p) + nblocks * block_size`). |
| 8 | Every returned block address must be **16-byte aligned**. |
| 9 | Handle edge cases: `nblocks == 0`, `block_size == 0` → return `NULL` from create. If `block_size` is not a multiple of 16, round it up (document this in your report). |

### Hints

- Write the three layout helpers from the lecture first:
  `pool_next_free()`, `pool_refcount()`, `pool_blocks()`.
  Everything else falls out of them.
- In `pool_blocks()`, align the **absolute address**, not the offset:
  `(char *)(((uintptr_t)m + 15) & ~(uintptr_t)15)`.
- Keep a debug function `kv_pool_dump()` (counts free blocks, walks the list)
  behind `#ifdef DEBUG` — it saves hours.

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
/* ... request runs ... */
arena_destroy(a);                       /* one call. done. */
```

---

## 📊 Task 3: Tests, Benchmark, Report (25%)

### 3a. Test suite — `test_kvpool.c`

Must cover, at minimum:

1. **Exhaustion:** allocate all blocks → next alloc returns `NULL`.
2. **Reuse:** free a block → next alloc returns the same address.
3. **Sharing:** alloc, `share`, free, free → block returns to the pool only after the second free (verify via reuse).
4. **Invalid free:** free a pointer from outside the pool and a misaligned pointer → warning printed, pool still consistent.
5. **Arena alignment:** every `arena_alloc` result satisfies `((uintptr_t)p % 16) == 0`.
6. **Arena reset:** after reset, the next alloc returns the arena's start address.
7. **Round trip:** 10 000 alloc/free cycles → pool ends with all blocks free.

### 3b. Benchmark — `bench.c`

Simulate a batch workload:

- `R = 1000` requests, block size `16384`, pool with `512` blocks.
- Each request: create a 4 KB arena, allocate 8–32 KV blocks (pick randomly),
  write one byte into each block, then free everything.

Measure both:

| Variant | How |
| --- | --- |
| **Your pool + arena** | as above |
| **libc baseline** | same workload using `malloc(block_size)`/`free` for the blocks |

Report: wall time (`clock_gettime(CLOCK_MONOTONIC)`) and peak RSS
(`getrusage(..., &ru)` → `ru_maxrss`). Present as a small table.

### 3c. Report — `REPORT.md`

Answer briefly (2–5 sentences each):

1. Why does the pool version fragment less than the `malloc` version?
2. Why must `kv_pool_destroy` recompute the total size instead of storing it? (Or did you store it — and at what cost?)
3. Where did the time go in each variant — allocation, freeing, or the workload itself? How do you know?
4. One design decision you made (block size, validation behavior, arena backing) and its tradeoff.
5. **Project connection:** what breaks if two threads call `kv_block_alloc` at once, and what is the minimal fix? (You don't implement it yet — Weeks 9–10.)

---

## 🚀 Bonus (up to +10%)

| Bonus | Description |
| --- | --- |
| **B1** | Double-free detection: freeing an already-free block prints a warning and does nothing. (How do you know it's free? Refcount is 0 — but be careful about race-free reasoning.) |
| **B2** | COW helper `void *kv_block_clone(KVPool *, void *ptr)`: if `refcount > 1`, allocate a fresh block, `memcpy`, decrement the old refcount, return the copy; otherwise return the same pointer. Test the diverging-prefix scenario. |
| **B3** | Thread-safety teaser: wrap alloc/free/share with a single `pthread_mutex_t` and stress-test with 4 threads. (We cover mutexes properly in Week 9.) |

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
grep -n "malloc\|calloc\|realloc\|free(" kvpool.c             # no libc heap in the pool
make bench && ./bench                           # table prints, no crashes
```

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
```

Then paste the repository link into Google Classroom.

**AI policy reminder (from the syllabus):** you may use AI tools for
explanation and debugging, but you must be able to explain *every line* you
submit — especially the layout arithmetic. List any AI assistance in
`REPORT.md`.

---

## ❓ FAQ

**Q: Can I use `sbrk` instead of `mmap`?**
No — the lab requires `mmap`. `mmap` regions can be returned cleanly with
`munmap`, and `sbrk` behaves badly once threads arrive (Week 9).

**Q: Why an index array for the free list instead of storing `next` pointers
inside free blocks (like the explicit free list in lecture)?**
You may implement that variant as a bonus, but the index array is the required
baseline: O(1), cache-friendly, and it never fights block alignment.

**Q: My `pool_blocks()` result isn't 16-byte aligned. Why?**
You aligned the *offset* instead of the *absolute address*. `data[]` does not
necessarily start at a 16-byte boundary (`offsetof(KVPool, data)` is 24 on
x86-64). Round the absolute pointer: `((uintptr_t)m + 15) & ~(uintptr_t)15`.

**Q: What should `kv_block_free` do with `NULL`?**
Same convention as libc `free`: silently ignore.

**Q: Does the arena need to grow when full?**
No. Return `NULL`. (Growth is a bonus if you want it — document the strategy.)

---

## 🔭 Where This Goes Next

| Week | What we add to this code |
| --- | --- |
| 9 | `pthread_mutex_t` around the pool (Week 9: synchronization) |
| 10 | A thread pool pulling requests off a queue, each with its arena (Week 10: producer/consumer) |
| 12 | Optional: shared-memory variant of the pool between processes (`mmap` + `MAP_SHARED`) |
| 13+ | Final project: a multithreaded, inference-like request server — this pool is its memory subsystem |

Keep a clean, tested copy of this lab. It is not throwaway code.