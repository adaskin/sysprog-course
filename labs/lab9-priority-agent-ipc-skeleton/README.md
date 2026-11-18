# Lab 9 Starter: Priority Agent Queues

See [`../9lab-priority-agent-ipc.md`](../9lab-priority-agent-ipc.md) for the full two-hour assignment.

## Build and run

```bash
make
./agentlab mutex 4 1000
./agentlab mq 4 1000
./agentlab shm 4 1000
make check       # quick correctness runs for all three backends
make compare     # supplied 1/2/4-agent timing comparison
```

The skeleton compiles as provided, but runs fail until the `TODO`s are completed. Do not change the files marked *given* in the assignment.

## Files

- `pool.c`: student agent and submitter loops (TODOs 1–2).
- `mutex_queue.c`: student mutex/condition-variable queue (TODOs 3–4).
- `mq_queue.c`: student POSIX message-queue operations (TODOs 5–6).
- `shm_queue.c`: student shared-memory/semaphore queue (TODOs 7–8).
- `agentlab.h`: common task, result, and backend contracts (given).
- `main.c`, `runner.c`, `work.c`: pool creation, tasks, timing, and validation (given).
- `compare.sh`: supplied comparison runs (given).

Only the three queue files and `pool.c` should need modification.

## How the run is tested (read this first)

1. `main.c` runs a **fast precheck** (`priority_order_ok`): submits 12 tasks
   and receives them, verifying priority (high first) and sequence (small first)
   tie-break. If this fails, the backend is broken — the pool is not involved yet.
2. `run_pool()` starts 1 submitter thread + N agents (threads for `mutex`,
   forked processes for `mq` and `shm`).
3. After the submitter finishes, the runner sends one priority-0 stop task per
   agent. Priority 0 is lowest, so every normal task drains first.
4. `main.c` then checks `results[i].done == 1` for all i and prints one CSV line.

## Debugging checklist

- **Program hangs?** Use `timeout 5 ./agentlab mutex 1 8`. A hang is almost
  always a missing `pthread_cond_signal`/`sem_post`, or a `while` replaced by `if`.
- **Wrong order?** `make check` fails loudly. Verify you use `best_task_index`
  in the array backends and that you do **not** use it for `mq`.
- **Garbage results?** The `results` array is `MAP_SHARED`, so children can write.
  Set `value`, then `agent`, then `done = 1` last.
- **Lock held while running a task?** The spec forbids it; `run_task()` must run
  after you unlock / post.
- If the mq backend fails with EINVAL, your kernel's `fs.mqueue.msg_max` is below `32`. 
  - Raise it: `sudo sysctl -w fs.mqueue.msg_max=32` 
  - If you cannot raise it, note that reducing `QUEUE_CAPACITY` is not enough on its own, because main.c's priority precheck submits a hardcoded batch of `12` tasks. The precheck will block at startup unless capacity ≥ 12 and msg_max ≥ capacity. The correct fix is to raise msg_max, not to shrink the queue.
  
## Hint for TODO 4 / TODO 8

After `best_task_index`, copy `tasks[best]` to `*task`, then move the last
element into the slot `best`, then decrement `count`. This preserves O(1)
removal and keeps the array compact.