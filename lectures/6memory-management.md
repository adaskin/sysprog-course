---
title: "Memory Management: Copy-on-Write, the Heap, and the KV Cache"
author: "Ammar Daskin"
marp: true
paginate: true
size: 16:10
theme: default
class: invert
---

# Memory Management
## Copy-on-Write, the Heap, Building an Allocator, and the KV Cache

---

The last two weeks we built the orchestrator: `fork`, `exec`, `wait` and
redirection in Week 4, then `dup2` and `pipe` in Week 5.

---

**Today**:

1. **What exactly did `fork()` copy?** 
   - The worker looked like a perfect copy of the orchestrator — but surely it did not duplicate gigabytes instantly?


2. **What does `malloc()` really do?** And `free()` returns memory... *to whom?*
    - Today we build our own allocator ideas 
   - and finish with the memory workload your projects will actually meet: 
     - the **KV cache**.

---

# 🎬 Cold Open: The $10,000 Question

Your orchestrator has loaded:

- a 2 GB model-weight file,
- a 500 MB embedding index,
- a 300 MB log cache.

Assume it calls 8 x `fork()`  to launch 8 worker agents.

- 🤔 The memory machine needs: 2.8 GB + 8 × 2.8 GB ≈ **25 GB**?

The machine is fine. Even `top` barely moves. Why?

---

# 🧠 The Answer is **copy-on-write (COW)**

`fork()` does not copy your memory. It *promises* to copy your memory.

- Parent and child get the same **virtual** address space.
- The bytes are only duplicated when somebody **writes**.
- Until then, all 2.8 GB are shared, read-only, between nine processes.

> it is the reason `fork()` + `exec()` is cheap enough to be the backbone of every shell, every CI system, and every agent orchestrator on Unix.

---

# 🎭 Act 1: Let's prove what `fork()` Actually Copies

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

🤔 What does the parent see for `x`? And look closely at the addresses too...

---

# 🔮 Act 1: Same Address, Different Value

```
child:  x = 42 at 0x7ffd3c2a1b4c
parent: x = 10 at 0x7ffd3c2a1b4c
```

🤯 The **same virtual address** holds **two different values** — one per process.

Before the explanation: can you state, in one sentence, what the kernel
must be doing behind this trick?

---

# 🧠 Act 1: The Mental Model of the Week

| Layer | Shared after fork? |
| --- | --- |
| Virtual addresses | yes — identical layout in both |
| Physical bytes | shared *until a write* (copy-on-write) |
| What each process sees | its own private copy |

The child's `x = 42` triggered a page copy — of just the 4 KB page containing `x`.
Everything else stayed shared. **The write is the bill collector.**

---

# 🔍 Act 1: One More Weird One

```c
fork();
int *p = malloc(100);
printf("pid %d got %p\n", getpid(), (void *)p);
```

Both processes run this after the fork. Both call `malloc(100)` independently.

🤔 Do they get the same address or different addresses?


---

# 🔍 Act 1: The Answer — Same Map, Different Territory

 **the same virtual address** — their heaps are identical copies with identical allocator state, so both hand out the same virtual pointer. 
 > But the bytes behind that pointer are now two separate physical pages.

**Virtual memory: same map, different territory.**

(Notice what just sneaked in: the allocator's own bookkeeping — free lists,
headers, the break — was copied by `fork()` too. Allocator state is just memory.)

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

Two copies in, two copies out.

🤔 Which of our Week 4 habits does this explain?

---

# 📚 Act 1: The Rules the Buffer Taught Us

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

---

# 🗺️ Act 2: Quick Check — the Answers

| Item | Lives in |
| --- | --- |
| `local` | stack |
| the pointer variable `p` itself | stack |
| the memory `*p` points to | heap |
| `"literal"` | read-only data (`.rodata`) |

One pointer, two regions: the variable on the stack, the bytes it points to
on the heap. (The full "where things live" table is Field Manual FM 10.)

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

# ⏸️ Act 3: Pause — It Works. So Why Is It Terrible?

60–90 seconds. In pairs: this allocator works. Name as many problems with it
as you can — be specific about what breaks, and when.

🤔 Your candidates, before you turn the page:

- ⁉️
- ⁉️
- ⁉️

Next slide: the bill arrives. Check how many you predicted.

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

# 🧱 Act 4: Inside `malloc` — The Heap Is a List of Blocks

The heap is not a shapeless blob. It is a **list of blocks**, each wearing a header:

```
[ header | payload bytes ........................ ]
[ header | payload ... ]
[ header | payload .............................. ]
   ↑ size + flags       ↑ what malloc returns (16-byte aligned)
```

🤔 Notice something: `malloc` returns a pointer to the payload — the bytes
you asked for. But then... where does it store the block's size? Somebody has
to remember how big each block is, or `free` could never find anything.

Think for ten seconds before the next slide.

---

# 🧱 Act 4: Inside `malloc` — The Header Struct

The size lives in a **header**, placed just before the payload:

```c
typedef struct {
    size_t size;          /* payload size; low bits store flags (ALLOC/FREE) */
    char   data[];        /* flexible array member — the payload (Week 2!) */
} block;
```

Why can flags hide in the low bits of `size`?
Because blocks are **aligned to 16-byte multiples**, so the low bits are always 0.

This is exactly the shape of the `Tag` + `Block` structs we wrote in class
for `mymalloc` — keep that code open, it comes back in Act 10.

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

🤔 What should `malloc` do with the leftover ~1000 bytes? Burning the whole
block would waste 97% of it; there must be a better move.

---

# ✂️ Act 4: Splitting — the Code

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

Merging matters because big holes serve big requests: two 8 KB splinters
side by side can satisfy a 12 KB request that neither could alone.

How do we find the **next** block? Walk forward by size. Easy.
How do we find the **previous** block? Walk *backward*... by what?

---

# ❓ Act 4: Pause — Finding the Previous Block

This is the puzzle the classic allocators had to solve. Take thirty seconds.

We know how to step forward: header at `b`, payload of `SIZE(b)` bytes,
next header right after.

But at `free(b)`, to merge backward we need the block *before* `b` —
its start address and its size. From `b` alone... how?

🤔 Hint: everything you need is allowed to live inside the heap itself —
in the blocks. What could you duplicate, and where?

Next slide: the classic answer — beautifully simple, easy to get wrong.

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

# 🕵️ Act 4: Pause — Our Search Walks Allocated Blocks, Too

Look back at `find_fit` (two slides ago). To find one free block,
we step over *every* block — allocated ones included.

🤔 As the heap fills up, most blocks are allocated. So our "find a free block"
search is mostly... walking over blocks we already know we can't use.

How would you skip them? Where would you keep the list of only the free ones —
and what memory would it cost?

Thirty seconds, then the classic trick.

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

# 🕳️ Act 6: Fragmentation — Predict the Wastes

Two wastes are possible in any allocator. Before naming them, spot them yourself.

**Case 1.** The allocator only hands out blocks in multiples of 16 KB.
Your program calls `malloc(2048)`.

```
[ allocated 16KB block ─── real use: 2KB ─── ? ]
```

🤔 What, exactly, is being wasted here? Whose fault is it — the program's or
the allocator's?

---

# 🕳️ Act 6: Internal Fragmentation

**Internal fragmentation** — the block is bigger than the request:

```
malloc(2KB) on a system handing out 16KB blocks:
[ allocated 16KB ─── real use 2KB ─── 14KB wasted inside ]
```

The 14 KB is inside an allocated block — marked used, unreachable to anyone.
It is not a bug; it is the price of fixed block sizes and alignment.

(Keep this picture — it comes back when we judge the KV pool at the end.)

Now the second case.

---

# 🕳️ Act 6: External Fragmentation — Predict

**Case 2.** The heap contains, in address order:

```
[ free 8KB ][ used 4KB ][ free 8KB ]
```

Your program calls `malloc(12KB)`.

🤔 There are 16 KB of free memory in total. Does the request succeed?
If not — why not, in one sentence?

---

# 🕳️ Act 6: External Fragmentation — and the 50% Rule

**External fragmentation** — enough free memory exists, but scattered:

```
[ free 8KB ][ used 4KB ][ free 8KB ]
→ 16KB free in total, yet malloc(12KB) FAILS
```

The "50% rule" intuition: first-fit leaves, on average, half-sized
splinters after each split. Splinters multiply. Eventually
`malloc` fails while `top` says memory is available. Users file bugs.

| Disease | Waste lives... |
| --- | --- |
| Internal fragmentation | *inside* allocated blocks (block > request) |
| External fragmentation | in *scattered free holes* that no request fits |

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

🤔 What is happening? Name the disease before the next slide.

---

# 🧠 Act 6: The Diagnosis

**External fragmentation**: the free list is full of holes that don't fit
incoming sizes. The allocator keeps growing the heap instead of reusing.

RSS climbs without a single leaked byte.

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

Read this program. It looks fine. It is not.

```c
int *array = malloc(2 * sizeof(int));
array[0] = 10; array[1] = 20;

array = realloc(array, 3 * sizeof(int));   /* 🤔 what if this returns NULL? */
array[2] = 30;                             /* segfault — and the old block is orphaned */
```

🤔 Trace the failure path out loud: what does `realloc` return when the heap
is exhausted, and what has just happened to our only pointer to the
original block?

---

# 💣 Act 7: Pause — What Just Broke?

Two independent disasters in two lines:

1. If `realloc` failed, it returned `NULL` — so `array = NULL`, and
   `array[2] = 30` writes to address 0. **Crash.**
2. Worse: assigning to `array` **overwrote our only pointer** to the
   original 2-int block. It is still allocated, still ours — and now
   unreachable. Even if we survive the crash, we can never `free` it. **Leak.**

Classic leak + crash combo, from one missing variable.

---

# 📐 Act 7: The Fix — Stage Through a Temporary

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
==3131==  40 bytes in 1 blocks are definitely lost   ← if we'd skipped free()
```

Compile with `-g` so the report names your source lines.
Cheap alternative for speed: `gcc -fsanitize=address` (ASan).

---

# ⏸️ Act 9: Pause — An Agent Leaks. Now What?

Your orchestrator supervises eight workers. One of them leaks — its RSS
climbs and climbs, and it is *your* machine it is climbing on.

🤔 From everything you know so far (Week 4's fork/exec gap, today's
`setrlimit`...), what can the orchestrator do — preferably *before* the leak
ever starts? Thirty seconds, in pairs.

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

# 🏟️ Act 9: Pause — The Request Allocation Pattern

Look at how a single agent task actually uses memory:

- it allocates many small objects while working,
- they are all needed only until the task ends,
- then none of them are needed. Ever again.

```c
char *prompt   = ...;   /* task starts  */
char *tokens   = ...;
char *scratch  = ...;
/* ... 40 more small allocations ... */
/* task ends: none of these are needed anymore */
```

🤔 Given that pattern... how many `free()` calls should a whole task need?
What would the allocation function look like if you exploited this?

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

This is the "tensor arena / memory pool" pattern — used by inference engines
(KV caches!), game engines, request servers. It is the soul of this week's lab,
and Act 10 shows you the workload that makes it necessary.

---

# 🤖 Act 10: The Ultimate Agent Workload — the KV Cache

In transformer-based inference, every active request keeps intermediate state
for the tokens processed so far — the **KV cache**.

We do **not** care how it is computed. This is a systems course, not an ML one.
Systems view only:

- a **large** memory region (often gigabytes),
- that **grows** as the sequence grows,
- is made of many **blocks**,
- may be **shared** between requests,
- and must be **freed** when the request ends.

That is not machine learning. That is *this lecture*: allocation, reuse,
sharing, fragmentation, cleanup.

---

# 🧮 Act 10: How Big Is "Big"?

```
KV bytes ≈ 2 × layers × kv_heads × head_dim × seq_len × dtype × batch
```

Rough 7B-class numbers:

```
layers = 32     kv_heads = 32     head_dim = 128
seq_len = 4096  dtype = 2 bytes   batch = 1

2 × 32 × 32 × 128 × 4096 × 2 ≈ 2 GiB  — per single request
```

Batch of 8 → **≈ 16 GiB**. One machine, a handful of concurrent agents.

🤔 Take 60 seconds and redo the plug-in yourself — then vary one knob
(e.g. batch = 32) and feel the number run away from you.

Even when the exact numbers change with every new model, the systems lesson
doesn't: memory like this cannot be managed casually.

---

# 🧱 Act 10: KV Blocks Are Allocator Blocks

Real serving engines manage KV memory in **fixed-size blocks**:

```c
typedef struct {
    uint32_t request_id, layer_id;
    uint32_t token_start, token_count;
    uint16_t refcount, flags;
    size_t   size;              /* payload bytes */
} KVHeader;

typedef struct {
    KVHeader hdr;
    char     data[];            /* flexible array member — Week 2! */
} KVBlock;
```

🤔 Week 2 quiz: where is the padding in `KVHeader`?

---

# 🧱 Act 10: The Padding — and the Costume

Between `flags` and `size`: `size_t` wants 8-byte alignment, but `flags`
ends at offset 20 — so the compiler inserts **4 bytes of padding**,
and `KVHeader` is 32 bytes, not 28.

Same lesson as Week 2's struct packing: if two programs disagree about this
layout, one of them is reading garbage.

And the bigger picture: this is the `Tag` + `Block` pair from our in-class
`mymalloc` code, wearing an inference costume.

---

# 🕵️ Act 10: Same Pointer Arithmetic, New Costume

```c
/* recover the header from a payload pointer */
KVBlock *b = (KVBlock *)((char *)payload - offsetof(KVBlock, data));

/* step to the next block */
KVBlock *next = (KVBlock *)((char *)b + offsetof(KVBlock, data) + b->hdr.size);
```

Contrast the dangerous style: `(KVBlock *)payload - 1` — works today,
breaks silently the day someone adds a field to the header. `offsetof` doesn't.

`mymalloc`'s `next_block()` did exactly this walk.
The names changed; the arithmetic didn't.

---

# ❓ Act 10: So... Just `malloc` It?

The naive design:

```c
char *kv = malloc(huge_size);
/* sequence grows... */
kv = realloc(kv, bigger);
```

🤔 Vote: what breaks **first** in a long-running server with many concurrent
requests? Name your candidate before the next slide.

---

# ❓ Act 10: What Breaks First — the Answers

- size changes → repeated `realloc` → copies + failure paths (Act 7!)
- thousands of fragments → external fragmentation (Act 6!)
- shared prefixes impossible — `malloc` doesn't know "shared"
- error/timeout paths leak: who frees 40 half-built caches?

Better tools exist. Two of them are already in this lecture: **pools** and **arenas**.

---

# 🗃️ Act 10: The Fixed-Size KV Pool — One Region

If every KV block is the same size (say 16 KB), **external fragmentation
vanishes**: any free block serves any request.

And we build the pool itself the way `Block` builds a block —
one allocation, one region, metadata inside:

```c
typedef struct {
    size_t   block_size;   /* payload bytes per block (e.g. 16384) */
    size_t   nblocks;
    uint32_t free_head;    /* first free block; UINT32_MAX = none  */
    char     data[];       /* everything lives here                */
} KVPool;
```

No `char *base`. No separately-malloc'd arrays. Struct + heap in **one `mmap`**.

---

# 📏 Act 10: Why the Struct Is Shaped Like This

- The **flexible array member must be last** — C allows exactly one, at the end.
- Fixed members are ordered largest-alignment first
  (`size_t`, `size_t`, `uint32_t`) → no padding wasted between them.
- The pool's address comes from `mmap` → page-aligned already.
- The free list is an array of **indices**, not a pointer chain —
  the explicit-free-list idea from Act 4, without fighting alignment.

One allocation, one `munmap`. Cleanup is trivially correct —
which is the entire philosophy of arenas, applied to the pool itself.

---

# 🗺️ Act 10: Inside `data[]`

```
data[]:
┌──────────────┬──────────────┬─────┬──────────┬──────────┬─────┐
│ next_free[]  │ refcount[]   │ pad │ block 0  │ block 1  │ ... │
│ nblocks × 4B │ nblocks × 2B │     │ 16 KB    │ 16 KB    │     │
└──────────────┴──────────────┴─────┴──────────┴──────────┴─────┘
                               ↑ rounded so block 0 is 16-byte aligned
```

```c
static uint32_t *pool_next_free(KVPool *p) { return (uint32_t *)p->data; }
static uint16_t *pool_refcount(KVPool *p) {
    return (uint16_t *)(p->data + p->nblocks * sizeof(uint32_t));
}
static char *pool_blocks(KVPool *p) {
    char *m = p->data + p->nblocks * (sizeof(uint32_t) + sizeof(uint16_t));
    return (char *)(((uintptr_t)m + 15) & ~(uintptr_t)15);  /* absolute! */
}
```

---

# ✏️ Act 10: Paper Exercise — Size the mmap

3 minutes, on paper, in pairs. No code. This is the exact computation the lab
will demand of you — get it wrong now, in class, where it's free.

`block_size = 16384`, `nblocks = 512`.

1. How many bytes do the `next_free` and `refcount` arrays occupy together?
2. `data[]` starts at offset `offsetof(KVPool, data)` = 24 from the pool's
   base. After the metadata arrays, what must happen before block 0 can start?
3. Write one expression for the total bytes `mmap` must request.

🤔 Compare your expression with your neighbor's. Do you round the offset
or the absolute address? Why does it matter?

Next slide: the reference implementation — check your arithmetic against it.

---

# ⚙️ Act 10: Building the Pool

```c
KVPool *kv_pool_create(size_t block_size, size_t nblocks) {
    size_t meta = nblocks * (sizeof(uint32_t) + sizeof(uint16_t));
    size_t total = sizeof(KVPool) + meta + 16 + nblocks * block_size;

    KVPool *p = mmap(NULL, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return NULL;
    p->block_size = block_size;  p->nblocks = nblocks;  p->free_head = 0;

    uint32_t *nf = pool_next_free(p);
    for (size_t i = 0; i + 1 < nblocks; i++) nf[i] = i + 1;  /* 0→1→2→… */
    nf[nblocks - 1] = UINT32_MAX;                            /* …→END   */
    memset(pool_refcount(p), 0, nblocks * sizeof(uint16_t));
    return p;
}
```

One region. The `+16` covers the alignment rounding. Later: one `munmap`.

---

# ⚡ Act 10: O(1) Alloc and Free

```c
void *kv_block_alloc(KVPool *p) {
    if (p->free_head == UINT32_MAX) return NULL;     /* pool exhausted */
    uint32_t i = p->free_head;
    p->free_head = pool_next_free(p)[i];             /* pop */
    pool_refcount(p)[i] = 1;
    return pool_blocks(p) + (size_t)i * p->block_size;
}

void kv_block_free(KVPool *p, void *ptr) {
    uint32_t i = ((char *)ptr - pool_blocks(p)) / p->block_size;
    uint16_t *rc = pool_refcount(p);
    if (rc[i] > 1) { rc[i]--; return; }              /* still shared */
    rc[i] = 0;
    pool_next_free(p)[i] = p->free_head;             /* push */
    p->free_head = i;
}
```

No splitting. No coalescing. No searching. Fixed sizes bought us all three.

---

# 🛡️ Act 10: Defensive Allocators — Predict

`kv_block_free` receives a raw pointer from the outside world. What if it
is wrong — a stack pointer, the middle of a block, another pool's block?

Blindly converting it to an index and pushing it onto the free list can
corrupt the whole pool.

🤔 Which checks would you perform, in what order, before trusting `ptr`?

---

# 🛡️ Act 10: The Validation Recipe

```c
void kv_block_free(KVPool *p, void *ptr) {
    char *base = pool_blocks(p);
    char *end  = base + p->nblocks * p->block_size;
    /* 1. inside the block region at all?          */
    if ((char *)ptr < base || (char *)ptr >= end) {
        fprintf(stderr, "kv_block_free: pointer outside pool\n"); return;
    }
    /* 2. exactly on a block boundary?             */
    if (((char *)ptr - base) % p->block_size != 0) {
        fprintf(stderr, "kv_block_free: misaligned pointer\n"); return;
    }
    /* 3. only now compute the index and proceed   */
    ...
}
```

Rule of systems programming: **at module boundaries, trust no pointer.**
Your lab's grader feeds bad pointers on purpose.

---

# 🤝 Act 10: Shared Prefixes — Predict

Two agents, same system prompt → same KV prefix.

🤔 Allocate it twice?

Think about Act 1 while you answer: what did the kernel do with the
orchestrator's read-only gigabytes?

---

# 🤝 Act 10: Refcounts — Share First, Copy on Mutation

No — we share it, and count the sharers:

```c
void kv_block_share(KVPool *p, void *ptr) {   /* refcount++ */
    pool_refcount(p)[block_index(p, ptr)]++;
}
```

- Reads share freely — cheap, exactly like Act 1's pages.
- A request that must **modify** a shared block copies it first, then diverges.

**Share the immutable, copy on mutation.**
The kernel did it for `fork()`; now we do it for KV blocks.
Same principle, different layer.

---

# 🔁 Act 10: The COW Rhyme — Same Idea, Two Layers

Step back and look at the whole lecture:

|  | Pages (Act 1) | KV blocks (Act 10) |
| --- | --- | --- |
| Shared until... | a write to a page | a mutation of a block |
| Mechanism | page table + fault | refcount + clone |
| Cost of sharing | ~zero | ~zero |
| Cost of the copy | one page, on first write | one block, on first mutation |

Copy-on-write was not a kernel trivia detail. It is a **design pattern**:
share what is immutable, duplicate lazily on divergence.

The kernel did it for `fork()`; we just did it for KV blocks.
Same principle, different layer — and you will meet it again wherever
large shared state meets occasional mutation.

---

# 🏟️ Act 10: The Full Request Lifetime

```
request starts →  arena_create()             (variable-size metadata)
                  kv_block_alloc() × N        (fixed-size KV blocks)
request runs   →  arena_alloc() bumps along; blocks fill up
request ends   →  arena_destroy()             — one free, metadata gone
                  kv_block_free() × N          — blocks return to the pool
```

- Metadata leaks are structurally impossible (arena).
- KV memory never touches `malloc` at all (pool).
- Timeout/crash cleanup = the same two lines, in a loop.

This is the shape of real serving engines. It is also this week's lab —
and the first brick of your end-of-semester project.

---

# ⚖️ Act 10: What the Pool Costs Us

| `malloc`-per-fragment | Fixed KV pool |
| --- | --- |
| any size, any time | one size per pool |
| pays per allocation (search, syscall risk) | O(1) pop/push |
| external fragmentation | none between blocks; internal if blocks oversized |
| no sharing | refcounted sharing |
| cleanup scattered | one `munmap` |

Real engines run **several** pools (multiple size classes — segregated lists,
Act 6!) and fall back to `mmap` for odd sizes. Nothing here is exotic;
it is this lecture, applied.

---

# 🦀 Rust Callout: Shared Blocks, Atomically

Our `refcount` array, as a type:

```rust
use std::sync::Arc;
let prefix: Arc<KVBlock> = Arc::new(compute_prefix());
let r1 = prefix.clone();   // refcount++ — cheap
let r2 = prefix.clone();   // refcount++ — cheap
// when the last Arc drops → memory freed. No kv_block_free to forget.
```

`Arc` = **atomic** reference counting = a thread-safe `refcount`.
Inference engines written in Rust manage shared prefixes exactly this way.
The C version is what you build in the lab.

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

Underneath, Rust calls a global allocator — historically the system's,
often `jemalloc` in practice. Same heap, different contract.

---

# 🧪 Lab 6: Build the KV Cache Memory Pool (1-Week Lab)

This week's lab is a take-home mini-project — and **Part 1 of the final
project's memory subsystem**. Full spec is in `lab6-kv-cache.md`; the API:

```c
KVPool *kv_pool_create(size_t block_size, size_t nblocks);
void   *kv_block_alloc(KVPool *pool);          /* NULL when exhausted */
void    kv_block_free(KVPool *pool, void *ptr);
void    kv_block_share(KVPool *pool, void *ptr);
void    kv_pool_destroy(KVPool *pool);          /* exactly one munmap */

Arena *arena_create(size_t cap);
void  *arena_alloc(Arena *a, size_t size);      /* 16-byte aligned */
void   arena_reset(Arena *a);
void   arena_destroy(Arena *a);
```

This is the code from Acts 9–10, re-typed by you, tested, benchmarked.
Everything you need is on these slides — no new ideas, only new keystrokes.

---

# 🧪 Lab 6: Rules of the Game

- pool internals: **exactly one `mmap`**, zero `malloc` calls in `kvpool.c`;
- O(1) alloc/free; 16-byte aligned blocks; Valgrind-clean test suite;
- benchmark against libc `malloc` (time + peak RSS) and write it up;
- bonus: double-free detection, COW-style cloning, `pthread_mutex_t` teaser.

The grader's first commands, worth memorizing:

```bash
grep -n "malloc\|calloc\|realloc\|free(" kvpool.c   # must find nothing
valgrind --leak-check=full ./test_kvpool            # must be silent
```

We wrote `mymalloc` together in class. Thursday you make the same ideas
serve a real workload.

---

# 🏁 Summary I: The Allocator's Story

| Mystery / breakage | Fix | Lesson |
| --- | --- | --- |
| `fork()` of a huge process is instant | copy-on-write | pages copy only on write |
| `printf` before fork prints twice | flush / `_exit` | stdio buffers are memory; fork copies them |
| `sbrk` on every allocation | free list | reuse beats syscalls |
| freed space unusable by big requests | splitting + coalescing | merge holes, split tails |
| heap full, yet `malloc` fails | fragmentation | size classes, arenas, better placement |
| `realloc` overwrites my only pointer | temp pointer + NULL check | the old block survives failure |
| agents eat all RAM | `setrlimit` in the fork/exec gap | limits are part of supervision |

---

# 🏁 Summary II: The Agents' Story

| Mystery / breakage | Fix | Lesson |
| --- | --- | --- |
| generic `malloc` crumbles under KV workloads | fixed-size pool | one size kills external fragmentation |
| two requests, one prompt | refcounted shared blocks | share immutable, copy on mutation |
| request metadata leaks everywhere | arena | many allocations, one free |
| bad pointers from callers | validate before trusting | defensive allocator boundaries |

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

---

# 📎 FM 11: The Single-Region Pool Pattern (Lab 6 Reference)

```c
typedef struct {
    size_t   block_size;
    size_t   nblocks;
    uint32_t free_head;
    char     data[];   /* [ next_free[] ][ refcount[] ][ pad ][ blocks... ] */
} KVPool;
```

| Quantity | Formula |
| --- | --- |
| free list offset | `p->data` |
| refcount offset | `p->data + nblocks * sizeof(uint32_t)` |
| first block | align up to 16: `(addr + 15) & ~15` (absolute address!) |
| block `i` | `pool_blocks(p) + i * block_size` |
| index from pointer | `(ptr - pool_blocks(p)) / block_size` |
| total for `munmap` | `(pool_blocks(p) - (char *)p) + nblocks * block_size` |

Rules: flexible array member last; one `mmap`/one `munmap`; `UINT32_MAX` = end
of free list; refcount `> 1` means "free just decrements" (COW sharing).

Field Manual ends. Remember: stories teach, manuals remind.