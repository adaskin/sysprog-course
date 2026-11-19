# Lab 9: Priority Agent Queues — Threads, IPC, and Shared Memory

**BIL 322 System Programming**
**Duration:** 2 hours
**Topic:** Synchronization, POSIX message queues, shared memory, and semaphores
**Starter:** [`lab9-priority-agent-ipc-skeleton/`](lab9-priority-agent-ipc-skeleton/)

## Goal

Build the same small agent system with three different queue implementations:

```text
                           +--> agent 0 --> results[task_id]
submitter --> task queue --+--> agent 1 --> results[task_id]
 (1 thread)                +--> agent 2 --> results[task_id]
                           +--> agent 3 --> results[task_id]
```

The agents represent workers in a small AI-agent orchestrator. Each task has a priority: urgent work should be taken before older, less urgent work. **No real AI service or external library is used.** The supplied `run_task()` function provides deterministic dummy work so that we can focus on system programming.

By the end of the lab, you should be able to:

1. Implement a blocking, thread-safe priority queue 
   1. Note yuo will not implement this through min-heap; you will uses a standard array: enqueue is O(1), dequeue is O(n)(best swapped with the last).
   2. Linux kernel implements this in ipc/mqueue.c through red-black trees: O(logn)
2. Use a submitter thread and a fixed pool of worker agents.
3. Replace a user-space queue with a Linux POSIX message queue.
4. Implement a process-shared queue in shared memory using semaphores.
5. Make a short, fair comparison of the three designs.

## What is given?

The [starter](lab9-priority-agent-ipc-skeleton/) supplies all code that is not the focus of this lab:

- task generation and deterministic dummy work;
- thread/process creation and clean shutdown;
- one submitter thread and one result slot per task;
- timing, correctness checks, and the comparison script;
- `best_task_index()`, which finds the highest-priority task in an array.

You complete only `pool.c`, `mutex_queue.c`, `mq_queue.c`, and `shm_queue.c`. There are eight marked `TODO`s. **A heap is not required:** with only 32 queue slots, a linear search keeps the lab focused on synchronization and IPC.

## Common rules

These rules are identical for all three backends:

- Queue capacity: **32 tasks**.
- Normal priorities: **1 = low, 2 = normal, 3 = high**. A larger value is taken first.
- Equal-priority tasks are ordered by their `sequence` value; smaller values come first.
- A task with `id == -1` and priority 0 is a **stop task**.
- The submitter creates normal tasks. The supplied runner sends one stop task per agent after submission finishes.
- `results[task.id]` belongs only to that task. Each agent writes directly to that slot; no results lock is needed.
- Do not create a new thread or process for every task. The agent pool size is fixed at startup.

| Backend | Queue owner | Agents | Synchronization/scheduling |
|---|---|---|---|
| `mutex` | Your array in process memory | Threads | `pthread_mutex_t`, two condition variables, `best_task_index()` |
| `mq` | Linux kernel | Processes | POSIX message-queue blocking and priorities |
| `shm` | Your array in shared memory | Processes | Three process-shared semaphores, `best_task_index()` |

## Suggested two-hour schedule

- **0–10 min:** Read `agentlab.h`, build the starter, inspect the TODOs.
- **10–50 min:** Common agent loops and the mutex/condition-variable backend.
- **50–75 min:** POSIX message-queue backend.
- **75–105 min:** Shared-memory/semaphore backend.
- **105–120 min:** Run the comparison and prepare a short demonstration.

## Task 1: Agent loops and a thread pool (45%)

### 1a. Complete the common loops — TODOs 1–2

In `pool.c`:

- `agent_loop()` repeatedly receives a task. It stops on a stop task; otherwise it runs the task and fills its result slot.
- `submitter_loop()` creates and submits task IDs `0` through `task_count - 1` in order.

Use the backend function pointers from `agentlab.h`. Do not call a concrete queue implementation directly from these loops.

### 1b. Complete the mutex priority queue — TODOs 3–4

In `mutex_queue.c`, implement blocking submit and receive operations using the given mutex and condition variables:

- A submitter waits while `count == capacity`.
- An agent waits while `count == 0`.
- Protect `count` and the task array with the mutex.
- Use a `while` loop around every condition-variable wait.
- After removing the best array entry, move the last entry into its place and decrement `count`.
- Wake waiters after changing the queue state. Do not hold the mutex while running a task.

Test this backend before continuing:

```bash
cd labs/lab9-priority-agent-ipc-skeleton
make
./agentlab mutex 1 128
./agentlab mutex 4 512
```

A correct run prints one CSV line and exits. Before timing the pool, the supplied checker submits a small batch and verifies priority and tie order. It also fails on a missing or duplicate task.

## Task 2: The same pool with a POSIX message queue (20%)

Complete TODOs 5–6 in `mq_queue.c` using `mq_send()` and `mq_receive()`:

- Send and receive one complete `task_t`; the queue attributes already have the correct message size.
- Pass `task->priority` to `mq_send()`.
- Retry a system call interrupted by a signal (`errno == EINTR`).
- Do **not** call `best_task_index()` here. The kernel selects the highest-priority queued message and preserves FIFO order within one priority.

The given initialization uses `mq_open()` and the given cleanup uses `mq_close()` and `mq_unlink()`. The process agents inherit the queue descriptor.

```bash
./agentlab mq 1 128
./agentlab mq 4 512
```

This lab uses **POSIX message queues** (`mq_*`), not the older System V interface (`msgget`, `msgsnd`, `msgrcv`).
> If the mq backend fails with EINVAL, your kernel's fs.mqueue.msg_max is below 32. Raise it: `sudo sysctl -w fs.mqueue.msg_max=32` If you cannot raise it, note that reducing QUEUE_CAPACITY is not enough on its own, because main.c's priority precheck submits a hardcoded batch of 12 tasks. The precheck will block at startup unless capacity ≥ 12 and msg_max ≥ capacity. The correct fix is to raise msg_max, not to shrink the queue.

## Task 3: The same pool in shared memory (25%)

Complete TODOs 7–8 in `shm_queue.c`.

The shared mapping and semaphore initialization are given. Notice the second argument to `sem_init()` is `1`, making the semaphores process-shared:

- `empty_slots` counts available array positions and starts at 32.
- `items` counts queued tasks and starts at 0.
- `lock` is a binary semaphore protecting the array and `count`.

Use the same priority-selection rule as Task 1. Wait for a slot/item **before** taking the array lock. Release the lock before posting the opposite counting semaphore. Retry `sem_wait()` after `EINTR`.

```bash
./agentlab shm 1 128
./agentlab shm 4 512
```

You may use the anonymous `MAP_SHARED` mapping already supplied; `shm_open()` is not needed because all process agents are children of the same parent.

## Task 4: Supplied comparison and short explanation (10%)

Run all correctness checks and the supplied comparison:

```bash
make check
make compare
# Optional, smaller comparison if the lab machines are slow:
sh compare.sh 5000
```

`compare.sh` uses the same tasks for every backend and varies only the queue design and agent count. Copy its CSV output into a short `NOTES.md` and answer these in **three to five sentences total**:

1. Which backend was fastest on your machine? Why might another machine or task size give a different result?
2. For each backend, who owns the queue and who blocks a full/empty-queue waiter?
3. Where is the priority rule implemented in each backend?

No graphs, large load experiments, or separate performance report are required.

## Demonstration and submission

Be prepared to demonstrate:

```bash
make
make check
make compare
```

Then show one queue operation from each backend and briefly explain its blocking and priority behavior. Submit your modified four source files and `NOTES.md`. Do not commit the compiled `agentlab` binary.

### Assessment

| Component | Weight |
|---|---:|
| Common agent/submitter loops and mutex thread pool | 45% |
| POSIX message-queue backend | 20% |
| Shared-memory/semaphore backend | 25% |
| Working demonstration and short comparison | 10% |

Correct, clean execution matters more than a large test suite. A faster backend is not worth extra credit by itself; your explanation should account for copying, kernel transitions, synchronization, and process/thread differences.

## Common mistakes

- Using `if` instead of `while` with `pthread_cond_wait()`.
- Sleeping on a condition variable without holding its mutex.
- Forgetting to wake either producers or consumers after changing the queue.
- Searching for the best task without holding the queue lock.
- Implementing a second priority queue around the POSIX message queue.
- Using `sem_init(..., 0, ...)` and expecting unrelated process agents to share the semaphore.
- Taking the array lock before waiting for a slot/item, causing a deadlock.
- Holding any queue lock while calling `run_task()`.

You may use AI tools for explanation and debugging under the course policy. You must be able to explain every line you submit, especially each wait, lock, unlock, signal, and post.
# Lab 9 — Visual Guide

## 0. What is a "backend"? 
```
          agent_loop()  /  submitter_loop()      ← student code
          ──────────────────────────────────
                 │  only these three calls
                 ▼
   ┌─────────────────────────────────────────────┐
   │   backend->submit (state, &task)  → 0 or -1 │
   │   backend->receive(state, &task)  → 0 or -1 │
   │   backend->destroy(state)                   │
   └────────────────────┬────────────────────────┘
                        │  one function pointer
                        ▼
    ┌──────────────┬──────────────┬──────────────┐
    │   mutex      │     mq       │     shm      │
    │ heap+condvar │   kernel     │ shm + sems   │
    └──────────────┴──────────────┴──────────────┘
```

---

## 1. The big picture

```
                ┌──────────────────┐
                │   submitter      │   1 thread
                │   submitter_loop │   make_task(0..N-1)
                └────────┬─────────┘
                         │ submit(&task)
                         ▼
        ╔══════════════════════════════════════════╗
        ║      PRIORITY QUEUE  (capacity = 32)     ║
        ║   highest priority first                 ║
        ║   smaller sequence breaks ties           ║
        ╚══════════════════════════════════════════╝
            │       │       │       │
            │ receive(&task) — blocks until a task exists
            ▼       ▼       ▼       ▼
    ┌────────┐┌────────┐┌────────┐┌────────┐
    │ agent0 ││ agent1 ││ agent2 ││ agent3 │  N fixed
    └───┬────┘└───┬────┘└───┬────┘└───┬────┘
        │         │         │         │
        ▼         ▼         ▼         ▼
    ┌───────────────────────────────────────────────┐
    │   results[task.id] = { value, agent, done=1 } │
    │   one slot per task ID → no lock needed       │
    └───────────────────────────────────────────────┘

   After submitter finishes, runner sends one priority-0 stop
   task per agent → agents exit → clean shutdown.
```

---

## 2. Three backends, three memory layouts

```
════════ MUTEX backend — one process, N threads ════════

   ┌─────────────────────────────────────────────────┐
   │                ONE PROCESS                      │
   │                                                 │
   │   [submitter]──┐                                │
   │                ▼                                │
   │       ┌────────────────────────────┐            │
   │       │ tasks[32]   count          │            │
   │       │ + mutex + 2 condition vars │  ← heap    │
   │       └───┬─────┬─────┬─────┬──────┘            │
   │           │     │     │     │                   │
   │        [a0]   [a1]   [a2]   [a3]  ← threads     │
   │           └──┬──┴──┬──┴──┬──┘                   │
   │              ▼     ▼     ▼                      │
   │       results[N]  (same heap — threads share it)│
   └─────────────────────────────────────────────────┘


════════ MQ backend — parent + N forked children ═══════

   ┌─── PARENT ───┐    ┌─── KERNEL ─────┐    ┌─ CHILDREN ─┐
   │  submitter   │    │   POSIX mq     │    │  agent 0   │
   │  thread ─────┼───▶│   capacity 32  │───▶│  agent 1   │
   │              │    │  priority in   │    │  agent 2   │
   │              │    │  the kernel    │    │  agent 3   │
   │  results ────┼────┼────────────────┼───▶│  writes    │
   │  (MAP_SHARED)│    │                │    │  results   │
   └──────────────┘    └────────────────┘    └────────────┘

   Name: /bil322_agents_<uid>_<pid>  (unique per process)


════════ SHM backend — parent + N forked children ══════

   ┌─── PARENT ───┐  ┌────── SHARED MAPPING ────────┐  ┌─ CHILDREN ─┐
   │  submitter   │  │  tasks[32]  count            │  │  agent 0   │
   │  thread ─────┼─▶│  sem lock    (binary, =1)    │─▶│  agent 1   │
   │              │  │  sem empty_slots (count,=32) │  │  agent 2   │
   │              │  │  sem items       (count,=0)  │  │  agent 3   │
   │  results ────┼──┼──(another MAP_SHARED)────────┼─▶│  writes    │
   │  (MAP_SHARED)│  └──────────────────────────────┘  │  results   │
   └──────────────┘                                    └────────────┘

   ⚠ sem_init(&s, 1, ...) — pshared MUST be 1 for this to work.
```

---

## 3. A task's life cycle (one trip through the pool)

```
submitter                      queue                agent
──────────                     ─────                ─────
  make_task(i)                                            
    │                                                     
    │  submit(&task)                                       
    ▼                                                     
  ┌──────────────────────┐                                
  │ if FULL: BLOCK       │  signal / post / mq wake  
  │ place into the queue │ ────────────────────────▶ 
  └──────────────────────┘                                 
                            receive(&task) ────▶┌────────────┐
                                if EMPTY: BLOCK │ find best  │
                                                │ swap-last  │
                                                └─────┬──────┘
                                                      ▼
                                                run_task(task)
                                                      ▼
                                    results[id].value = v
                                    results[id].agent = a
                                    results[id].done  = 1  ← last
```

---

## 4. Priority — what "highest priority first" actually means

`make_task(id)` sets `priority = ((id*13) % 3) + 1`. So priorities cycle `1, 2, 3, 1, 2, 3, ...`.

```
   id:     0    1    2    3    4    5    6    7
   prio:   1    2    3    1    2    3    1    2
   seq:    0    1    2    3    4    5    6    7

   QUEUE CONTENT after all 8 submitted (array order):

   ┌────┬────┬────┬────┬────┬────┬────┬────┐
   │p1s0│p2s1│p3s2│p1s3│p2s4│p3s5│p1s6│p2s7│
   └────┴────┴────┴────┴────┴────┴────┴────┘

   best_task_index() scans:  highest prio, then lowest seq
   ORDER DELIVERED:
      2, 5,       ← all p=3, sequence ascending
      1, 4, 7,    ← all p=2, sequence ascending
      0, 3, 6     ← all p=1, sequence ascending
```

### How removal works — swap-with-last

```
   BEFORE remove (pick idx 2):

   ┌────┬────┬────┬────┬────┬────┬────┬────┐
   │p1s0│p2s1│p3s2│p1s3│p2s4│p3s5│p1s6│p2s7│  count=8
   └────┴────┴────┴────┴────┴────┴────┴────┘
              ▲
              best=2

   Step 1: *task       = tasks[2]            (copy out)
   Step 2: tasks[2]    = tasks[--count]      (last → hole)
   Step 3: --count done in step 2

   AFTER remove (count=7):

   ┌────┬────┬────┬────┬────┬────┬────┐
   │p1s0│p2s1│p2s7│p1s3│p2s4│p3s5│p1s6│
   └────┴────┴────┴────┴────┴────┴────┘
              ▲
              p2s7 moved here from end

Note: The lab does not use a heap. 
It uses a fixed-size array with a linear scan to find the best task
O(n) scan, O(1) swap.
- you can implement min-heap if you want, that would make O(logn).
```

---

### Note that Linux mqueue is implemented by rb-trees
```
   msg_tree (rb-tree keyed by priority)          rightmost cached
              │                                       │
              ▼                                       ▼
        ┌──────────┐       ┌──────────┐       ┌──────────┐
        │ p=1 node │       │ p=2 node │       │ p=3 node │  ← msg_tree_rightmost
        │ msg_list │       │ msg_list │       │ msg_list │
        └────┬─────┘       └────┬─────┘       └────┬─────┘
             │                  │                  │
          msg A              msg B              msg E   ← head (oldest, served next)
          msg C              msg D              msg F
                                                msg G   ← tail (newest)
```
---

## 5. When does `submit` / `receive` block?

```
════════════════════════ submit() blocks WHEN... ═════════════════════

   mutex        while (count == capacity)
                    pthread_cond_wait(&not_full, &lock);     ← HERE
                ...store...
                pthread_cond_signal(&not_empty);

   mq           mq_send(desc, &task, sizeof, prio)           ← HERE
                (returns EAGAIN unless O_BLOCK; blocks by default)

   shm          sem_wait(&empty_slots)                       ← HERE
                sem_wait(&lock)
                ...store...
                sem_post(&lock)
                sem_post(&items)


════════════════════════ receive() blocks WHEN... ════════════════════

   mutex        while (count == 0)
                    pthread_cond_wait(&not_empty, &lock);    ← HERE
                ...pick best, swap-last, count--...
                pthread_cond_signal(&not_full);

   mq           mq_receive(desc, &task, sizeof, NULL)        ← HERE

   shm          sem_wait(&items)                             ← HERE
                sem_wait(&lock)
                ...pick best, swap-last, count--...
                sem_post(&lock)
                sem_post(&empty_slots)
```

---

## 6. Stop task / shutdown

```
  After submitter finishes:

       runner.c sends ONE stop task per agent
       ┌──────────┐  submit   ┌──────────────┐  receive   ┌──────────┐
       │  runner  │──────────▶│    QUEUE     │───────────▶│ agent i  │
       │ (parent) │ id=-1,p=0 │  (sinks to   │   p = 0    │  sees    │
       └──────────┘ per agent │   bottom)    │  (lowest)  │  id==-1  │
                              └──────────────┘            │ → exits  │
                                                          └──────────┘

  KEY: priority 0 is LOWER than every normal task (1,2,3),
       so stops are only dequeued AFTER all real work has drained.

  Queue just before stops:   [p3 p3 p2 p1 p1 ... ]
  Queue after M stops added: [p1 p1 p0 p0 p0 ... p0]
                                   ▲  ▲
                             normal  M stop tasks
```

---

## 7. The precheck — why capacity 10 hung

```
════════════ priority_order_ok()  (runs BEFORE any agent starts) ════════════

  for i = 0..11:   submit(make_task(i))      ← 12 submits first
  for i = 0..11:   receive(&task); check     ← 12 receives come AFTER

  With capacity = 10:

      submit#0 .. submit#9  ──▶ fills all 10 slots  ✓
      submit#10             ──▶ BLOCKS forever
                                   (nobody is receiving yet)

  ⚠ capacity must be ≥ 12 for this precheck to pass.

  With capacity = 32  →  ✓ (but the kernel default msg_max = 10 blocks
                             mq_open with EINVAL unless raised)
```
```bash
cat /proc/sys/fs/mqueue/msg_max
cat /proc/sys/fs/mqueue/msgsize_max
sudo sysctl -w fs.mqueue.msg_max=32
```
---

## 8. Top 3 bugs — visually

```
════ BUG A: if() instead of while() on cond_wait ════

   ✗  if (count == capacity)
          pthread_cond_wait(&not_full, &lock);   ← check ONCE

   ┌─────────────────────────────────────────────────┐
   │ spurious wake, or another producer grabs the    │
   │ slot before this thread reacquires the lock →   │
   │ proceeds anyway → count = capacity+1 → overflow │
   └─────────────────────────────────────────────────┘

   ✓  while (count == capacity)
          pthread_cond_wait(&not_full, &lock);   ← always re-check
```

```
════ BUG B: missing wake-up ════

   submit():  lock, count++, unlock         ─── no signal(&not_empty)

   consumer:  lock, while(count==0) cond_wait    ─── sleeps forever
              ─────────────────────
              queue HAS items, nobody told the consumer
              → timeout 5 ./agentlab ... is your friend
```

```
════ BUG C: wrong semaphore wait order (shm, TODO 7) ════

   ✗ lock first, then empty_slots

     producer A:  sem_wait(&lock)         ✓ got it
                  sem_wait(&empty_slots)  ✗ blocked — queue full
     consumer B:  sem_wait(&lock)         ✗ blocked — needs the lock
                                                to free a slot
     ┌────────────────────────────────────────────────────┐
     │ A holds lock, waits for a slot                     │
     │ B needs lock to free a slot                        │
     │ → DEADLOCK                                          │
     └────────────────────────────────────────────────────┘

   ✓ empty_slots first, then lock

     producer A:  sem_wait(&empty_slots)  ✗ blocked
     consumer B:  sem_wait(&items)        ✓
                  sem_wait(&lock)         ✓
                  remove, sem_post(&lock)
                  sem_post(&empty_slots) ──▶ A wakes
     ┌────────────────────────────────────────────────────┐
     │ A never held the lock while blocking → no deadlock │
     └────────────────────────────────────────────────────┘
```

---

## 9. What the lock protects — and what it must not

```
═══════════════════════ LOCK SCOPE ═══════════════════════

  INSIDE the lock                   OUTSIDE the lock
  ────────────────                  ────────────────
  • count                           • run_task(&task)    ← slow work
  • tasks[] array reads & writes    • results[id] = ...
  • best_task_index()               • make_task(i)
                                    • printf / perror
                                    • munmap, mq_close, exit

  Why? Holding the lock around run_task() serialises all agents
  and turns the pool into a single worker. Parallelism comes from
  many agents running tasks simultaneously — that only works if
  the lock is released before running.

  results[id]: each task owns exactly one slot. Two agents never
  touch the same slot → no lock needed. MAP_SHARED is what makes
  the writes visible from child processes (mq, shm backends).
```

---

## 10. Threads vs processes — memory view

```
═════════════════════════════════════════════════════════════════════
  THREADS (mutex)                     PROCESSES (mq, shm)
═════════════════════════════════════════════════════════════════════

  Same address space:                 Separate address spaces:

   ┌──────────────────────┐            ┌────────────┐   ┌────────────┐
   │  submitter thread    │            │   parent   │   │  child 0   │
   │  agent0 thread       │            │  submitter │   │  agent 0   │
   │  agent1 thread       │            │  + results │   │            │
   │  agent2 thread       │            │  mapping   │   │            │
   │  agent3 thread       │            └─────┬──────┘   └─────┬──────┘
   │                      │                  │                │
   │  ── ALL SHARE ──     │                  │  fork() inherits
   │     the queue        │                  │  MAP_SHARED mappings
   │     the results      │                  ▼                ▼
   └──────────────────────┘            ┌───────────────────────────┐
                                       │     shared physical       │
   pthread_mutex works                 │     pages (results, shm    │
   (all in one pid)                    │     queue, semaphores)    │
                                       └───────────────────────────┘

                                       comms across processes:
                                         mq:  via kernel
                                         shm: via shared pages +
                                              process-shared sems
```

---

## 11. Full run timeline

```
  main
   │
   ├─ 1. priority_order_ok(name)            ← 12-submit precheck
   │        backend opened, checked, destroyed
   │
   ├─ 2. open_backend(name)                 ← fresh queue created
   │
   ├─ 3. mmap results[N]   (MAP_SHARED|ANON) ← visible to future children
   │
   ├─ 4. run_pool(backend, agents, N)
   │      │
   │      ├─ spawn N agents          ─┐
   │      ├─ spawn submitter thread    │  all concurrent
   │      │   (submits 0..N-1)         │
   │      ├─ join submitter            │
   │      ├─ submit N stop tasks       │
   │      ├─ join / waitpid N agents  ─┘
   │      └─ return 0
   │
   ├─ 5. scan results[N]: count done==1, XOR checksum
   │
   ├─ 6. print one CSV row: backend,agents,N,ms,tasks/sec,checksum
   │
   └─ 7. destroy backend, munmap results, exit
```

---

## Quick-fire recap (end-of-class summary on the board)

| Question   | mutex | mq | shm |
|---|---|---|---|
| Where does the queue live? | process heap | kernel | `MAP_SHARED` memory |
| Who wakes a blocked receiver? | `pthread_cond_signal(&not_empty)` | kernel on `mq_send` | `sem_post(&items)` |
| Who wakes a blocked submitter? | `signal(&not_full)` | kernel on `mq_receive` | `sem_post(&empty_slots)` |
| Who picks the highest priority? | user code (`best_task_index`) | **the kernel** | user code (`best_task_index`) |
| Runs on signals? | no | yes (`EINTR` retry) | yes (`EINTR` retry) |
| `pshared` flag? | n/a (threads) | n/a | **must be `1`** |
| Capacity limit? | heap | `fs.mqueue.msg_max` | heap |

