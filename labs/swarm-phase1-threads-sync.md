# Swarm Phase 1: Leaders as Processes, Followers as Threads

**BIL 322 System Programming — Final Project, Phase 1**
**Duration:** 1 week
**Project:** [19 Mayıs Drone Show](../projects/project3-drone-show-19-mayis.md)
**Submission:** GitHub Classroom (repository) + Google Classroom link

> **Status: DRAFT.** Details, numbers, and the skeleton code are finalized the
> week this phase is assigned.

---

## 🎯 Context

The show has a center, leaders, and followers. In this phase everything runs on one
machine and the pieces talk the way you have already learned:

- the **center** forks **leader processes** and talks to them over **pipes** (Labs 4 and 5),
- each leader runs **follower threads** that move in lockstep, one **beat** at a time,
- the center's **inference service** is a thread pool behind a bounded request queue, and
  every request gets memory from your **Lab 6 pool**.

This is the thread-and-synchronization half of the project: queues, locks, condition
variables, semaphores, a barrier, and your first swarm-flavored deadlock.

## 📚 Learning Objectives

1. Build a **bounded, thread-safe queue** with a mutex and condition variables.
2. Run a **thread pool** that consumes the queue and shuts down cleanly.
3. Use a **counting semaphore** to limit concurrent work and a **barrier** to keep threads in lockstep.
4. Compare **one lock** against **per-row locks** on the occupancy grid and measure the difference.
5. Define a **lock ordering** and use it to avoid deadlock.
6. Keep a multi-process, multi-threaded C program clean under Valgrind and ThreadSanitizer.

## 🧰 Prerequisites

- Week 7-8 lectures (threads, mutexes, condition variables, semaphores, barriers, deadlock).
- Lab 4 (`fork`/`exec`/`waitpid`), Lab 5 (pipes), Lab 6 (`kvpool`).

---

## 🏗️ Architecture for This Phase

```text
center (parent process)
 ├─ main thread: reads the score, sends THEME, reads leader pipes
 ├─ inference pool: N threads  ◀── bounded request queue (PLAN_REQ)
 ├─ occupancy grid (one drone per cell) + SDL view
 └─ pipes ──▶ leader 0 (child process)
              ├─ main thread: protocol with the center, validates plans
              └─ follower threads F0..Fk, one barrier per beat
```

The protocol is the one in the project spec: `HELLO`, `ASSIGN`, `THEME`, `PLAN_REQ`,
`PLAN`, `BUSY`, `READY`, `DONE`, `SHUTDOWN`. It is a **fixed contract**: you should be
able to test a leader by typing these lines into its pipe by hand. Write the send and
receive code against **file descriptors** so Phase 3 can swap pipes for sockets.

---

## 🛠️ Task 1: The Bounded Queue (25%)

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
- After `queue_close`, pops drain the remaining items and then return -1. Pushes return -1 immediately.
- No busy-waiting, no `sleep` for synchronization.

Use it as the center's **request queue** (`queue_try_push` → `BUSY` when full).

## 🧠 Task 2: The Inference Service and the Leader Agent (25%)

- **Center:** a pool of planner threads pops requests, allocates a request arena from the
  Lab 6 pool (pool size is a command-line option), calls `plan_section`, replies with
  `PLAN`, and releases the arena. A full queue replies `BUSY <retry_ms>`. A semaphore
  limits how many planner calls run at once (`--max-inflight`).
- **Leader:** on `ASSIGN` and `THEME`, sends `PLAN_REQ`, waits for `PLAN`, runs the
  **validator** (speed ≤ 2 cells per beat, no two drones in a cell, inside the zone),
  and asks again with a new `style_seed` if the plan is invalid. After `BUSY`, it retries
  with a **randomized** delay (not a fixed one — see the livelock slide). Then it sends `READY`.

## 🥁 Task 3: Followers in Lockstep and the Occupancy Grid (30%)

- The center sends the beat clock (a local timer thread for now; the tick length is a
  command-line option, `0` for tests). Each follower thread moves toward its keyframe for
  the next beat, then waits at a **barrier** shared by the leader's followers.
- Before moving into a cell a follower must **claim** it in the occupancy grid
  (`grid_claim(x, y, id)`), and release the cell it leaves. A failed claim means the
  follower waits for the next beat instead of busy-waiting.
- Implement **two** versions of the grid behind a compile-time flag:
  1. one mutex for the whole grid,
  2. one mutex per row.

  Write a headless benchmark (`make bench`) that performs the given long score with 4
  leaders × 16 followers at tempo 0 and prints the time of each version. In the report,
  explain the numbers (or the lack of difference) and what else could be the bottleneck.

## ⚠️ Task 4: Find the Deadlock (10%)

The skeleton's `--swap` mode (a "criss-cross" move in the finale) makes two adjacent
followers exchange cells, locking both rows. With `--swap` enabled, the program can hang.

1. Reproduce the hang. Show the two stuck threads (`gdb -p` and `thread apply all bt`).
2. Name the Coffman condition you will break, and fix it with a lock ordering.
3. Explain in the report why your fix cannot create a *livelock*.

## 📊 Task 5: Tests and Report (10%)

- `make test` runs headless: queue unit tests (order, blocking, close), a 4-leader run of the
  short score whose performance score must reach the given threshold with **zero safety
  violations**, and a clean-shutdown test.
- Run once under ThreadSanitizer (`-fsanitize=thread`) and once under Valgrind.
- `REPORT.md`: architecture sketch, what each mutex protects, lock ordering, the
  deadlock write-up, benchmark table, AI assistance.

---

## 🚀 Bonus (up to +10%)

- B1: A priority request queue (the next section's leader first) that does not starve other leaders.
- B2: Reader-writer lock for the grid, with the viewer thread as the reader.
- B3: A battery counter per follower and a charging-station semaphore.

## ✅ Grader's Checklist

```bash
make && make test
make tsan && ./run_tsan_test.sh        # no data races reported
valgrind --leak-check=full --error-exitcode=1 ./center --no-gui --test
```

Zero-tolerance items: busy-waiting, an `if` around `pthread_cond_wait`, missing
`queue_close` handling, a hang on shutdown, a missing report.

## 🔭 Where This Goes Next

Phase 2 moves the stage state into shared memory and the viewer into its own process;
Phase 3 replaces the pipes with sockets. Keep `queue.c`, the protocol code, and the
tests clean.
