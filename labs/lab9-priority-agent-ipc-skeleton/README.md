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
