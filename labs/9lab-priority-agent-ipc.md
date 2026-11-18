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

1. Implement a blocking, thread-safe priority queue.
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
