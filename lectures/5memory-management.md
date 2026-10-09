---
title: "Memory Management: Copy-on-Write, the Heap, and Building an Allocator"
author: "Ammar Daskin"
marp: true
paginate: true
size: 16:10
theme: default
class: invert
---

<style type="text/css">
div {
font-size: clamp(10px, 2vw, 28px);
text-align: left;
}
img {
display: block;
width: 70%;
text-align: center;
}
</style>

# Memory Management
## Copy-on-Write, the Heap, and Building an Allocator
### Week 6

Last week we built the orchestrator out of `fork`, `exec`, `wait`, `dup2`, `pipe`.
Two black boxes stayed sealed, each with a note taped on it: "open in Week 5."

1. **What exactly did `fork()` copy?** The worker looked like a perfect copy of the orchestrator — but surely it did not duplicate gigabytes instantly?
2. **What does `malloc()` really do?** And `free()` returns memory... *to whom?*

Today we open both boxes. Then we build our own `malloc` — and meet every way it can fail.

---

# 🎬 Cold Open: The $10,000 Question

Your orchestrator has loaded:

- a 2 GB model-weight file,
- a 500 MB embedding index,
- a 300 MB log cache.

Now it calls `fork()` eight times to launch eight worker agents.

🤔 How much memory does the machine need?
2.8 GB + 8 × 2.8 GB ≈ **25 GB**?

The machine is fine. Even `top` barely moves. Why?

---

# 🧠 The Answer Is a Lazy Lie

`fork()` does not copy your memory. It *promises* to copy your memory.

- Parent and child get the same **virtual** address space.
- The bytes are only duplicated when somebody **writes**.
- Until then, all 2.8 GB are shared, read-only, between nine processes.

This is **copy-on-write** (COW), and it is the reason `fork()` + `exec()`
is cheap enough to be the backbone of every shell, every CI system,
and every agent orchestrator on Unix.

Let's prove it exists — by breaking our assumptions about it.

---

# 🎭 Act 1: What Does `fork()` Actually Copy?

A prediction exercise. Run this in your head first:

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int x = 10;
    pid_t pid = fork();
    if (pid == 0) {                 /* child */
        x = 42;
        printf("child:  x = %d at %p\n", x, (void *)&x);
        _exit(0);
    }
    wait(NULL);
    printf("parent: x = %d at %p\n", x, (void *)&x);
    return 0;
}
```

🤔 What does the parent see for `x`? And look closely at the addresses...

---

# 🔮 Act 1: Same Address, Different Value

```
child:  x = 42 at 0x7ffd3c2a1b4c
parent: x = 10 at 0x7ffd3c2a1b4c
```

🤯 The **same virtual address** holds **two different values** — one per process.

This is the core mental model of the week:

| Layer | Shared after fork? |
| --- | --- |
| Virtual addresses | yes — identical layout in both |
| Physical bytes | shared *until a write* (copy-on-write) |
| What each process sees | its own private copy |

The child's `x = 42` triggered a page copy — of just the 4 KB page containing `x`.
Everything else stayed shared. The write is the bill collector.

---

# 🔍 Act 1: One More Weird One

```c
fork();
int *p = malloc(100);
printf("pid %d got %p\n", getpid(), (void *)p);
```

Both processes run this after the fork. Both call `malloc(100)` independently.

🤔 Do they get the same address or different addresses?

Answer: **the same address** — their heaps are identical copies with identical
allocator state, so both hand out the same virtual pointer. But the bytes behind
that pointer are now two separate physical pages.

Virtual memory: same map, different territory.

---

# 📚 Act 1: The Buffer That Printed Twice

Week 4's `_exit(127)` mystery, finally explained. Predict this one:

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("launched agent");      /* no \n — stays in the stdio buffer */
    fork();
    return 0;                      /* exit() flushes the buffer... */
}
```

🤔 What appears on stdout?

---

# 💥 Act 1: `launched agentlaunched agent`

The stdio buffer lives **in process memory** (user space, in the `FILE`'s buffer).
`fork()` copied it — buffer and all — into the child.
Both processes then ran `exit()`, and both flushed *their* copy.

That is why:

- the failed-exec child in Week 4 calls `_exit(127)`, never `exit(127)` —
  `exit` would flush the inherited buffer and duplicate the orchestrator's output;
- long-running agents call `fflush(stdout)` (or use `write`) —
  buffered bytes are invisible to anyone polling the log file;
- **everything you've allocated is part of what fork copies**: heap blocks,
  pointers, allocator metadata. All of it — lazily.

Rule: **memory state is process state.** `fork()` inherits all of it.

---

# 🦀 Rust Callout: No Fork in Safe Rust

Rust's standard library refuses to expose raw `fork()` — and now you can guess why.
A forked child inherits mutex states, allocator state, open fds... a minefield
that safe Rust cannot audit. So:

```rust
Command::new("worker").spawn()?;   // fork+exec, hidden and controlled
```

If you truly need `fork` in Rust, it lives in `libc::fork()` behind `unsafe` —
with a "you own every invariant now" warning implied.

(Course note: 🦀 boxes are pattern recognition only. You won't write Rust here.)

---

# 🗺️ Act 2: The Address Space, Revisited

Week 2's memory layout, now with the week's two protagonists labeled:

```
low addresses                                              high addresses
[ text ][ data ][ bss ][ heap ━━━━━━━━━▶ ]      [ ◀━━━━━━━━━ stack ]
                         ↑ program break           (grows downward)
                       (brk lives here)
```

| Region | Grows? | Managed by |
| --- | --- | --- |
| text / data / bss | fixed | loader |
| heap | expandable | **`malloc` — today's subject** |
| stack | grows down | kernel, per thread |

The heap at the system level is a contiguous range of addresses that the program
can expand or contract. The border it expands is the **program break**.

🤔 Quick check from Week 2: where do `local`, `*p`, and `"literal"` live?
(stack / heap / read-only data.)

---

# 🎛️ Act 3: Asking the Kernel for More Heap — `brk`/`sbrk`

```c
#include <unistd.h>

int   brk(void *addr);               /* set the program break */
void *sbrk(intptr_t increment);      /* move it by a delta; returns old break */
```

```c
void *top_of_heap  = sbrk(0);        /* where is the break now? */
sbrk(1024);                          /* grow by 1 KiB */
malloc(16384);                       /* malloc may grow it further */
void *top_of_heap2 = sbrk(0);
printf("Heap grew from %p to %p\n", top_of_heap, top_of_heap2);
```

Notes:

- Program break = end of the process's data segment.
- You should almost never call these directly — use `malloc`.
- Freshly grown memory is **not zeroed** by your code's standards;
  the kernel gives you zero pages, but reuse via an allocator needs care.

---

# 🧱 Act 3: The World's Dumbest `malloc`

If the heap is just "move the break," then:

```c
void *malloc(size_t size) {
    void *p = sbrk(size);
    if (p == (void *)-1)   /* no memory */
        return NULL;
    return p;
}

void free(void *p) {
    /* Does nothing. */
}
```

It even works! So why does libc ship a 10,000-line allocator instead?

---

# 💸 Act 3: The Bill for the Dumb Allocator

Three problems, and all three are *this week*:

1. **Every `malloc` is a system call.** Mode switch, kernel work, back.
   Millions of small allocations → death by a million traps.
2. **`free` does nothing.** Freed blocks are never reused — a program that
   allocates/frees in a loop grows without bound until the OOM killer arrives.
3. **The break only really grows.** Shrinking it back is fragile;
   memory handed to the OS via `sbrk` rarely goes home.

Real `malloc` fixes #1 and #2 with one idea:
**keep freed memory in a free list and hand it out again.**
The kernel is only bothered when the list runs dry.

---

# 🧰 Act 4: The C Allocation API

```c
#include <stdlib.h>

void *malloc(size_t size);                         /* uninitialized */
void *calloc(size_t nmemb, size_t size);           /* zeroed, overflow-checked */
void *realloc(void *ptr, size_t newsize);          /* resize */
void  free(void *ptr);                             /* return to the allocator */
```

Safe allocation pattern — non-negotiable:

```c
int *ptr = malloc(sizeof(int));
if (!ptr) {
    fprintf(stderr, "Allocation failed!\n");
    exit(EXIT_FAILURE);
}
```

❗ Always verify the return value before use.
❗ `malloc` memory is uninitialized — `memset` (or `calloc`) when you need zeros.

---

# 🧱 Act 4: Allocating Structures

```c
struct foo *ptr = malloc(sizeof(struct foo));
if (ptr == NULL) abort();
memset(ptr, 0, sizeof(struct foo));      /* zero initialization */

/* array of pointers */
int *ptrs[10];
ptrs[0] = malloc(sizeof(int));
ptrs[1] = malloc(sizeof(int));

/* pointer to pointer */
int **dbl_ptr = malloc(sizeof(int *));
```

And a hand-rolled `calloc` — malloc + zeroing:

```c
void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *mem = malloc(total);
    return mem ? memset(mem, 0, total) : NULL;
}
```

✅ Zero-initialization, ✅ modern `calloc` also checks `nmemb * size` for overflow.

---

# 🪦 Act 4: What `free()` Actually Does

Read this carefully — it is the whole act in one paragraph:

- Usually, `free` lets a **later** `malloc` reuse the space.
- In the meantime, the space stays in your process as part of a
  **free list** used internally by `malloc`.
- Occasionally, `free` can actually return memory to the operating system
  and shrink the process — but do not count on it.
- **Any use of a pointer that refers to freed space is undefined behavior.**

```c
int *ptr = malloc(sizeof(*ptr));
do_something(ptr);
free(ptr);
ptr = NULL;        /* habit: a freed pointer should become a dead pointer */
```

`free(p)` does not erase your pointer. It erases your *right to dereference it*.

---

# 🔗 Act 4: Freeing Structures — Order Matters

```c
struct chain {
    struct chain *next;
    char *name;
};

void free_chain(struct chain *head) {
    while (head) {
        struct chain *temp = head;
        head = head->next;
        free(temp->name);   /* free contained data first */
        free(temp);
    }
}
```

🔁 **Golden rule: free child resources before the parent structure.**
Free `temp` first and `temp->name` becomes a dangling dereference.

🤔 Orchestrator translation: a Task owns its log path, its command line,
its report buffer. Tear down the children, then the Task, then the table slot.

---

# 🧱 Act 4: Inside `malloc` — Blocks and Headers

The heap is not a shapeless blob. It is a **list of blocks**, each wearing a header:

```
[ header | payload bytes ........................ ]
[ header | payload ... ]
[ header | payload .............................. ]
   ↑ size + flags       ↑ what malloc returns (16-byte aligned)
```

A minimal header:

```c
typedef struct {
    size_t size;          /* payload size; low bits store flags (ALLOC/FREE) */
    char   data[];        /* flexible array member — the payload (Week 2!) */
} block;
```

Why can flags hide in the low bits of `size`?
Because blocks are **aligned to 16-byte multiples**, so the low bits are always 0.

---

# 🕵️ Act 4: The Implicit Free List

Simplest navigation scheme: no list of free blocks at all.
Just walk **every** block in address order using the size field:

```c
block *heap_start;                    /* first block, from sbrk/mmap */

static block *next_block(block *b) {
    return (block *)(b->data + SIZE(b));   /* step over payload */
}

static block *find_fit(size_t size) {
    for (block *b = heap_start; b < heap_end; b = next_block(b))
        if (!IS_ALLOC(b) && SIZE(b) >= size)
            return b;                      /* first fit! */
    return NULL;                           /* time to grow the heap */
}
```

`malloc` = find a free block that fits (+ split it if oversized).
`free` = clear the ALLOC bit (+ merge with neighbors — coming up).

---

# ✂️ Act 4: Splitting — Don't Waste the Tail

A 1 KB free block can serve a 24-byte request... but it shouldn't, wholesale:

```
before:  [ header | 1024 bytes free ............................. ]
malloc(24):
after:   [ header | 24 bytes alloc ][ header | 976 bytes free .. ]
```

```c
static void split(block *b, size_t size) {
    if (SIZE(b) < size + MIN_BLOCK) return;        /* remainder too small */
    block *rest = (block *)(b->data + size);
    rest->size  = SIZE(b) - size - sizeof(block);  /* new free block */
    b->size     = size | ALLOC_FLAG;
}
```

Without splitting, every small allocation causes **internal fragmentation** —
the block is marked used, and the wasted tail is unreachable.

Alignment note: payload starts must stay 16-byte aligned.
Unaligned access costs performance — or crashes on some architectures.

---

# 🧲 Act 4: Coalescing — Holes Want to Merge

`free` does more than flip a bit. Two adjacent free blocks are really one block:

```
before free(b):  [ A alloc ][ b alloc ][ C free ]
after free(b):   [ A alloc ][ b free  ][ C free ]   ← two holes...
coalesce:        [ A alloc ][ b + C merged free ]   ← ...one bigger hole
```

How do we find the **next** block? Walk forward by size. Easy.
How do we find the **previous** block? Walk *backward*... by what?

---

# 🏷️ Act 4: Boundary Tags — Headers at Both Ends

Duplicate the header at the **end** of each block:

```
[ btag | payload .................. | btag ]
   ↑ header copy                      ↑ footer copy
```

```c
typedef struct { size_t size_flags; } btag;

#define FOOTER(b)   ((btag *)((char *)(b) + sizeof(btag) + SIZE(b)))
/* the previous block's footer sits directly before our header: */
#define PREV_TAG(b) ((btag *)(b) - 1)
```

On `free(b)`:

- look at `FOOTER(b) + 1` (next header) → merge forward if free;
- look at `PREV_TAG(b)` (previous footer) → merge backward if free.

O(1) in both directions. That is what "pointer arithmetic and coalescing"
in the allocator-challenges list actually means.

---

# 🗃️ Act 4: Explicit Free Lists — Skip the Busy Blocks

The implicit list walks *everything* — allocated or not. Slow when the heap is
mostly full. Faster: keep a linked list of **only free blocks**:

```c
struct block {
    size_t info;              /* e.g. size in 16-byte units, low bits = flags */
    struct block *next;       /* next FREE block (lives inside the payload!) */
    char data[];
};
```

Trick worth savoring: a free block's payload is unused — so the `next` pointer
lives **inside the payload itself**. Zero extra memory.

- `malloc`: search the free list only.
- `free`: insert into the free list (at head = O(1); in address order = easy coalescing).

Variants you'll meet in the wild: segregated lists, slabs, arenas — Act 6.

---

# 🎯 Act 5: Where Does a Block Go? Placement Strategies

A request arrives; several holes could hold it. Which one wins?

| Strategy | Rule |
| --- | --- |
| First fit | first hole big enough — stop searching |
| Next fit | like first fit, but resume where the last search ended |
| Best fit | smallest hole that fits |
| Worst fit | largest hole available |

🤔 Vote: which would you ship? Which sounds smart but isn't?

---

# ⚖️ Act 5: The Trade-Off Table

| Strategy | Search cost | Notes |
| --- | --- | --- |
| First fit | O(n) | simple, fast enough; fragments the front of the heap |
| Next fit | O(1)* | faster still, but spreads fragmentation everywhere |
| Best fit | O(n)** | tightest fit; leaves a dust of tiny unusable holes |
| Worst fit | O(n)** | keeps holes large; rarely practical in practice |

\* amortized by remembering position. ** improvable with a min/max heap or size-indexed lists.

The uncomfortable truth: **optimal allocation is the knapsack problem — NP-hard.**
Every real allocator is a bundle of heuristics tuned for a *workload*:

- allocation is application-dependent;
- minimize fragmentation, keep search fast, keep coalescing cheap — pick your balance.

---

# 🕳️ Act 6: Fragmentation — the Allocator's Two Diseases

**Internal fragmentation** — the block is bigger than the request:

```
malloc(2KB) on a system handing out 16KB blocks:
[ allocated 16KB ─── real use 2KB ─── 14KB wasted inside ]
```

**External fragmentation** — enough free memory exists, but scattered:

```
[ free 8KB ][ used 4KB ][ free 8KB ]
→ 16KB free in total, yet malloc(12KB) FAILS
```

The "50% rule" intuition: first-fit leaves, on average, half-sized
splinters after each split. Splinters multiply. Eventually
`malloc` fails while `top` says memory is available. Users file bugs.

---

# 🧠 Act 6: Predict the Failure

A worker agent runs for three days. It allocates and frees constantly:

```c
while (running) {
    char *msg  = malloc(random_size());   /* 16 B .. 4 KB */
    process(msg);
    free(msg);
}
```

RSS (resident memory) climbs slowly forever. No leak — every `malloc` is freed.

🤔 What is happening, and which disease is it?

External fragmentation: the free list is full of holes that don't fit
incoming sizes. The allocator keeps growing the heap instead of reusing.
This is why long-lived servers care deeply about allocator design —
and why you sometimes see a periodic "restart to reclaim memory" ritual.

---

# 🛠️ Act 6: The Professional Countermeasures

**Segregated (size-class) lists** — one free list per size class:

```
16B | 32B | 64B | 128B | 256B | ...
```

Search is nearly O(1), and same-sized holes reuse each other perfectly.

**Buddy allocator** — split blocks in powers of two; a block's *buddy*
(the other half) is computable by XOR, so coalescing is instant:

```
1024 → 512 + 512 → 256 + 256 + 512
```

**Slab allocation** — pre-allocated caches of fixed-size objects
(the kernel's own trick for `task_struct`-sized things).

**Arena allocators** — one big region, bump-pointer allocation, free everything
at once. (Our agents will love this one — Act 9.)

**Memory tagging** — e.g. ARM MTE: hardware tags each allocation to catch
use-after-free at runtime. Security meets the allocator.

---

# 📐 Act 7: `realloc` — the Three Outcomes

```c
void *realloc(void *ptr, size_t newsize);
```

A naive mental model: allocate new, copy `min(old, new)`, free old.

```c
void *result = malloc(newsize);
memcpy(result, ptr, newsize < oldsize ? newsize : oldsize);
free(ptr);
return result;
```

Reality has **three outcomes**, and only two of them are moves:

1. **Same place** — the block can grow in situ (next block is free). Pointer unchanged.
2. **New place** — a bigger block elsewhere; data copied; old block freed.
3. **NULL** — failure. **The old block is still alive and still yours.**

Outcome 3 is where the famous bug lives. Next slide.

---

# 💣 Act 7: The One-Line Memory Leak

```c
int *array = malloc(2 * sizeof(int));
array[0] = 10; array[1] = 20;

array = realloc(array, 3 * sizeof(int));   /* 🤔 what if this returns NULL? */
array[2] = 30;                             /* segfault — and the old block is orphaned */
```

If `realloc` fails, it returned `NULL` — and we just overwrote our only
pointer to the original block. Classic leak + crash combo.

The fix — always stage through a temporary:

```c
void *tmp = realloc(array, 3 * sizeof(int));
if (tmp == NULL)      { /* handle failure; array is still valid! */ }
else if (tmp == array){ /* grew in place */ array[2] = 30; }
else                  { array = tmp;       array[2] = 30; }
```

A safe wrapper (from the field):

```c
void *xrealloc(void *ptr, size_t size) {
    void *value = realloc(ptr, size);
    if (value == NULL) perror("Virtual memory exhausted");
    return value;
}
```

---

# 🗺️ Act 7: Growing Up — `sbrk` vs `mmap`

Where does `malloc` get fresh memory? Two doors:

| `sbrk` | `mmap` |
| --- | --- |
| No longer POSIX-standard | POSIX-compliant |
| Easy to expand the heap | Handles large blocks efficiently |
| Cannot really return memory to the OS | `munmap` returns memory to the OS cleanly |

Modern allocators use both:

- small requests → the `sbrk`-backed heap with free lists;
- large requests (glibc default threshold: **128 KB**) → private `mmap` regions,
  which can be handed straight back on `free`.

That is the "occasionally, `free` actually shrinks the process" clause, mechanized.

---

# 🧟 Act 8: The Heap Failure Taxonomy

You already met these in Week 2's bug list. Now you know the machinery they break:

| Bug | Mechanism | Symptom |
| --- | --- | --- |
| Memory leak | `free` never called; block unreachable | RSS climbs forever |
| Use-after-free | pointer used after `free` | garbage, crashes, **exploits** |
| Double free | same pointer freed twice | allocator metadata corrupted |
| Heap overflow | write past the payload | clobbers the *next block's header* |

The last one is why heap bugs are security bugs: overflow a payload and you
rewrite a header — now `malloc` trusts attacker-shaped size fields.

Defenses: `free(p); p = NULL;`, bounds checks, Valgrind, ASan (Field Manual FM 7).

---

# 🕵️ Act 8: Valgrind Reads the Allocator's Diary

```c
int *p = malloc(10 * sizeof(int));
p[10] = 42;              /* one past the end */
free(p);
p[0] = 1;                /* use after free */
```

```
$ valgrind --leak-check=full ./a.out
==3131== Invalid write of size 4
==3131==    at 0x1091A2: main (buggy.c:4)
==3131==  Address 0x4a5a048 is 0 bytes after a block of size 40 alloc'd
==3131== Invalid write of size 4            ← use after free
==3131== 40 bytes in 1 blocks are definitely lost   ← if we'd skipped free()
```

Compile with `-g` so the report names your source lines.
Cheap alternative for speed: `gcc -fsanitize=address` (ASan).

---

# 🤖 Act 9: Memory Management for Agents

Why does an orchestrator care about all this? Three direct applications:

**1. Isolation is a feature.** Each worker is a process → separate address space.
A leaking or corrupted agent dies alone; the orchestrator survives.
(Week 4's "processes vs threads" table, now with full understanding.)

**2. Limits between fork and exec — the gap again!**

```c
#include <sys/resource.h>
struct rlimit r = { 256*1024*1024, 256*1024*1024 };
setrlimit(RLIMIT_AS, &r);        /* cap this agent's address space at 256 MB */
execvp(program, argv);
```

An agent that leaks toward 256 MB gets `ENOMEM` instead of taking the machine down.

**3. OOM is real.** If the fleet leaks past physical memory, the kernel's
OOM killer picks a victim by score. Guess whose agent framework it often picks.

---

# 🏟️ Act 9: Arena Allocators — the Agent Pattern

Agents do short-lived, batch-style work: allocate many objects during a task,
need none of them after it. So stop freeing individually:

```c
typedef struct { char *buf; size_t used, cap; } Arena;

void *arena_alloc(Arena *a, size_t n) {
    n = (n + 15) & ~(size_t)15;              /* align to 16 */
    if (a->used + n > a->cap) return NULL;   /* or grow the arena */
    void *p = a->buf + a->used;
    a->used += n;
    return p;
}
```

Task ends → `free(a->buf)`. **One free.** Leaks are structurally impossible.

This is the "tensor arena / memory pool" pattern from the course plan —
used by inference engines (KV caches!), game engines, request servers.
It is also the soul of this week's lab.

---

# 🦀 Rust Corner: Allocation With Guardrails

C's discipline, as types:

```rust
let p: Box<i32> = Box::new(42);      // malloc + initialize...
drop(p);                             // ...and free, but automatic at scope end
// double free? use after free? Neither is expressible in safe Rust.

let mut v: Vec<u8> = Vec::new();
v.push(1);                           // grows by doubling — realloc underneath
```

| C | Rust |
| --- | --- |
| `malloc` returns `NULL` on failure | allocation failure **aborts** by default |
| `free(p); /* p still usable */` | `drop` at scope end; no manual call |
| realloc dance with `tmp` | `Vec`/`String` handle growth internally |

Underneath, Rust calls a **global allocator** — historically the system's,
often `jemalloc` in practice. Same heap, different contract.

---

# 🧪 Lab: Build Your Own Allocator

This week's lab is the assignment specification. In C, no libc `malloc`
inside your implementation (wrap it with `-Dmalloc=mymalloc` style tricks or
a test harness):

```c
void *mymalloc(size_t size);
void  myfree(void *ptr);
void *myrealloc(void *ptr, size_t size);
```

Requirements:

- implicit free list + first fit (bonus: explicit list, best fit);
- splitting with a minimum block size; coalescing on free (boundary tags);
- 16-byte aligned payloads;
- grow the heap with `sbrk` (bonus: `mmap` for big requests);
- a Valgrind/ASan-clean test suite: alloc/free loops, realloc growth, fragmentation stress.

Benchmark against libc `malloc`. Losing is expected; understanding is the point.
(This lab is the seed of the upcoming memory-allocator programming assignment.)

---

# 🏁 Summary: The Story So Far

| Mystery / breakage | Fix | Lesson |
| --- | --- | --- |
| `fork()` of a huge process is instant | copy-on-write | pages copy only on write |
| `printf` before fork prints twice | flush / `_exit` | stdio buffers are memory; fork copies them |
| `sbrk` on every allocation | free list | reuse beats syscalls |
| freed space unusable by big requests | splitting + coalescing | merge holes, split tails |
| heap full, yet `malloc` fails | fragmentation | size classes, arenas, better placement |
| `realloc` overwrites my only pointer | temp pointer + NULL check | the old block survives failure |
| agents eat all RAM | `setrlimit` in the fork/exec gap | limits are part of supervision |

Next week: **threads** — one address space, many actors, and every rule from
today's "separate memory" column quietly retired. Bring your mutexes.

---

# 📖 Field Manual (Reference Appendix)

Lookup material for labs, homework, and exams. Not narrated in class —
consult it like a man page.

---

# 📎 FM 1: The Allocation API

```c
#include <stdlib.h>
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

/* aligned variants */
void *aligned_alloc(size_t alignment, size_t size);        /* C11 */
int   posix_memalign(void **memptr, size_t align, size_t size);
```

| Rule | Detail |
| --- | --- |
| `malloc` | uninitialized; may return `NULL` |
| `calloc` | zeroed; checks `nmemb*size` overflow |
| `realloc(NULL, n)` | same as `malloc(n)` |
| `realloc(p, 0)` | implementation-defined; avoid |
| `free(NULL)` | safely does nothing |
| payload alignment | at least 16 bytes on modern glibc |

---

# 📎 FM 2: `brk` / `sbrk`

```c
#include <unistd.h>
int   brk(void *addr);              /* set program break */
void *sbrk(intptr_t increment);     /* returns OLD break; (void*)-1 on error */
```

- `sbrk(0)` reports the current break without moving it.
- Growing past the break asks the kernel to extend the data segment.
- Not POSIX anymore; allocators use it internally, applications shouldn't.

---

# 📎 FM 3: `mmap` for Anonymous Memory

```c
#include <sys/mman.h>
void *mmap(NULL, length, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
int   munmap(void *addr, size_t length);   /* really returns memory */
```

| Flag | Meaning |
| --- | --- |
| `MAP_ANONYMOUS` | not backed by a file; fresh zero pages |
| `MAP_PRIVATE` | copy-on-write semantics |
| `PROT_READ/WRITE` | page permissions |

Also the foundation of shared memory IPC (`MAP_SHARED`, Week 12) and
memory-mapped files.

---

# 📎 FM 4: Alignment Rules (Recap from Week 2)

| Type | Typical alignment |
| --- | --- |
| `char` | any address |
| `short` | even (2) |
| `int`, `float` | 4 |
| `long`, `double`, pointers | 8 |
| malloc payloads | 16 (glibc) |

Allocator consequences:

- block sizes are kept in 16-byte units → low bits free for flags;
- splitting must preserve payload alignment;
- `#define ALIGN16(n) (((n) + 15) & ~(size_t)15)` is your friend.

---

# 📎 FM 5: Placement Strategy Cheat Sheet

| Strategy | Search | Fragmentation profile |
| --- | --- | --- |
| First fit | from start | front-loaded fragmentation |
| Next fit | resume position | spread fragmentation |
| Best fit | full scan / heap | dust of tiny holes |
| Worst fit | full scan / max-heap | large survivors, rarely best |

All O(n) scans can be improved with size-indexed structures
(min/max heaps, segregated lists). Optimal offline allocation = knapsack = NP-hard.

---

# 📎 FM 6: Fragmentation Glossary

| Term | Meaning |
| --- | --- |
| Internal fragmentation | allocated block larger than request |
| External fragmentation | free memory exists but is non-contiguous |
| Blowup | worst-case growth due to fragmentation |
| Splitting | dividing an oversized free block |
| Coalescing | merging adjacent free blocks |
| Program break | end of the `sbrk`-managed heap |
| Copy-on-write | pages duplicated only on first write |
| Arena | bulk-alloc region freed in one shot |

---

# 📎 FM 7: Heap Debugging Toolkit

| Tool | Command | Finds |
| --- | --- | --- |
| Valgrind | `valgrind --leak-check=full ./prog` | leaks, invalid r/w, use-after-free |
| ASan | `gcc -fsanitize=address -g` | same classes, ~2× faster runs |
| glibc checks | `MALLOC_CHECK_=3 ./prog` | some heap corruption, loudly |
| GDB | `break malloc` / `watch *p` | allocation flow inspection |

Compile with `-g` always when debugging. No symbols, no line numbers, no mercy.

---

# 📎 FM 8: Tuning Knobs (glibc)

| Knob | Effect |
| --- | --- |
| `mallopt(M_MMAP_THRESHOLD, n)` | size above which allocations use `mmap` |
| `mallopt(M_TRIM_THRESHOLD, n)` | when to try returning heap to OS |
| `mallopt(M_ARENA_MAX, n)` | cap allocator arenas (threads!) |
| `malloc_trim(0)` | explicitly ask for memory return |

Environment equivalents: `MALLOC_MMAP_THRESHOLD_`, `MALLOC_ARENA_MAX`, etc.
You will rarely need these — but server memory mysteries often end here.

---

# 📎 FM 9: Memory Limits for Agents

```c
#include <sys/resource.h>
struct rlimit rl = { .rlim_cur = limit, .rlim_max = limit };
setrlimit(RLIMIT_AS, &rl);      /* address space cap — set before exec */
```

| Limit | Caps |
| --- | --- |
| `RLIMIT_AS` | total virtual address space |
| `RLIMIT_DATA` | data segment (heap-ish) |
| `RLIMIT_STACK` | stack size |

Shell side: `ulimit -v 262144` (KiB). Container side: cgroups v2 `memory.max`.
The OOM killer scores processes (`/proc/<pid>/oom_score_adj`) when the
machine runs dry — supervision includes expecting that call.

---

# 📎 FM 10: Where Things Live (Master Table)

| Item | Region |
| --- | --- |
| machine code | text |
| initialized globals/statics | data |
| zero-initialized globals/statics | bss |
| string literals | rodata |
| local variables, frames | stack |
| `malloc`/`calloc` blocks | heap (or `mmap`) |
| the pointer variable itself | stack (or wherever declared) |
| `FILE` buffers | heap (inside the `FILE` struct's buffer) |

`fork()` copies all of it — lazily. `exec()` replaces all of it — except fds.

Field Manual ends. Remember: stories teach, manuals remind.