# Swarm Phase 1: Leaders as Processes, Followers as Threads

**BIL 322 System Programming — Final Project, Phase 1**
**Duration:** 1 week
**Project:** [Drone Swarm Painters](../projects/project3-drone-swarm-painters.md)
**Submission:** GitHub Classroom (repository) + Google Classroom link

> **Status: DRAFT.** Details, numbers, and the skeleton code are finalized the
> week this phase is assigned.

---

## 🎯 Context

Your swarm has a center, leaders, and followers. In this phase everything runs on
one machine and the pieces talk the way you have already learned:

- the **center** forks **leader processes** and talks to them over **pipes**
  (Labs 4 and 5),
- each leader runs **follower threads** that pull work from a shared queue,
- the center's **planner** is a thread pool behind a bounded request queue, and
  each request gets memory from your **Lab 6 pool**.

This is the thread-and-synchronization half of the project: queues, locks, condition
variables, semaphores, and your first swarm-flavored deadlock.

## 📚 Learning Objectives

1. Build a **bounded, thread-safe queue** with a mutex and condition variables.
2. Run a **thread pool** that consumes the queue and shuts down cleanly.
3. Use a **counting semaphore** to limit concurrent work.
4. Compare **one canvas lock** against **per-row locks** and measure the difference.
5. Define a **lock ordering** and use it to avoid deadlock.
6. Keep a multi-process, multi-threaded C program clean under Valgrind and ThreadSanitizer.

## 🧰 Prerequisites

- Week 7-8 lectures (threads, mutexes, condition variables, semaphores, deadlock).
- Lab 4 (`fork`/`exec`/`waitpid`), Lab 5 (pipes), Lab 6 (`kvpool`).

---

## 🏗️ Architecture for This Phase

```text
center (parent process)
 ├─ main thread: assigns regions, reads leader pipes
 ├─ planner pool: N threads  ◀── bounded request queue (PLAN_REQ)
 ├─ canvas (grid of colors) + SDL view
 └─ pipes ──▶ leader 0 (child process)
              ├─ main thread: protocol with the center
              └─ follower threads F0..Fk ◀── task queue (cells to paint)
```

The protocol is the one in the project spec: `HELLO`, `ASSIGN`, `PLAN_REQ`,
`PLAN`, `BUSY`, `PAINTED`, `DONE`, `SHUTDOWN`. Write the send and receive code
against **file descriptors** so Phase 3 can swap pipes for sockets.

---

## 🛠️ Task 1: The Bounded Queue (30%)

Implement `queue.c` with this interface (given in `queue.h`):

```c
typedef struct queue queue_t;

queue_t *queue_create(size_t capacity, size_t item_size);
int      queue_push(queue_t *q, const void *item);      /* blocks while full  */
int      queue_try_push(queue_t *q, const void *item);  /* 0 ok, -1 if full   */
int      queue_pop(queue_t *q, void *out);              /* blocks while empty */
void     queue_close(queue_t *q);                       /* wake everyone up   */
void     queue_destroy(queue_t *q);
```

Rules:

- `while`, not `if`, around every `pthread_cond_wait`.
- After `queue_close`, pops drain the remaining items and then return -1.
  Pushes return -1 immediately.
- No busy-waiting, no `sleep` for synchronization.

Use it twice: as the center's **request queue** (`queue_try_push` → `BUSY` when full)
and as each leader's **task queue**.

## 🧵 Task 2: Followers and the Planner Pool (30%)

- **Leader:** on `ASSIGN`, sends `PLAN_REQ`, waits for `PLAN`, pushes the cells into
  its task queue, and starts `n_followers` threads. A follower pops a cell, "flies"
  there (move one cell per tick; the tick length is a command-line option, `0` for tests),
  and reports `PAINTED` to the leader's main thread, which forwards it to the center.
- **Center:** a pool of planner threads pops requests, allocates a request arena from the
  Lab 6 pool (pool size is a command-line option), calls `plan_region`, replies
  with `PLAN`, and releases the arena. A full queue replies `BUSY <retry_ms>`; the
  leader must retry with a **randomized** delay (not a fixed one — see the livelock slide).
- Use a **semaphore** to limit how many planner calls run at once
  (`--max-inflight`).

## 🔒 Task 3: The Canvas and Lock Granularity (20%)

The center applies `PAINTED` messages to the canvas. Implement **two** versions
behind a compile-time flag:

1. one mutex for the whole canvas,
2. one mutex per row.

Write a headless benchmark (`make bench`) that paints a 512×512 image using 4
leaders × 16 followers with tick length 0 and prints the time of each version.
In the report, explain the numbers (or the lack of difference) and say what
else could be the bottleneck.

## ⚠️ Task 4: Find the Deadlock (10%)

The skeleton's `swap` mode lets two followers exchange targets when they are
adjacent, locking the two cells' rows. With `--swap` enabled, the program can hang.

1. Reproduce the hang. Show the two stuck threads (`gdb -p` and `thread apply all bt`).
2. Name the Coffman condition you will break, and fix it with a lock ordering.
3. Explain in the report why your fix cannot create a *livelock*.

## 📊 Task 5: Tests and Report (10%)

- `make test` runs headless: queue unit tests (push/pop order, blocking, close),
  a 4-leader painting run whose final canvas must match the target image within the
  given tolerance, and a clean-shutdown test.
- Run once under ThreadSanitizer (`-fsanitize=thread`) and once under Valgrind.
- `REPORT.md`: architecture sketch, what each mutex protects, lock ordering, the
  deadlock write-up, benchmark table, AI assistance.

---

## 🚀 Bonus (up to +10%)

- B1: A priority request queue (leaders with larger regions get planned first) that
  does not starve small regions.
- B2: Reader-writer lock for the canvas, with a viewer thread as the reader.
- B3: Followers that recharge: a battery counter, a charging station semaphore.

## ✅ Grader's Checklist

```bash
make && make test
make tsan && ./run_tsan_test.sh        # no data races reported
valgrind --leak-check=full --error-exitcode=1 ./center --no-gui --test
```

Zero-tolerance items: busy-waiting, an `if` around `pthread_cond_wait`, missing
`queue_close` handling, a hang on shutdown, a missing report.

## 🔭 Where This Goes Next

Phase 2 moves the canvas into shared memory and the viewer into its own process;
Phase 3 replaces the pipes with sockets. Keep `queue.c`, the protocol code, and the
tests clean.
