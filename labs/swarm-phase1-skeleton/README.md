# Swarm Phase 1 skeleton — threads and synchronization

BIL 322, Final Project (Autonomous Drone Swarm Choreographers), Phase 1.
The assignment is in `labs/swarm-phase1-threads-sync.md`, the project in
`projects/project3-drone-swarm-choreographers.md`. Read both before you start.

## Build and run

```bash
make                 # builds center, leader, swapstress, gridbench and the unit tests
make test            # the checks in tests/run_tests.sh (PASS/FAIL per line)
make tsan            # the same show under ThreadSanitizer (must print no WARNING)
make bench           # one global lock vs per-row locks, 1..8 threads (Task 3)

./center --tempo 0                                   # fast lockstep run (what the tests use)
./center --ascii 8                                   # real time, draw the stage every 8 beats
./center --battery 1:20                              # leader 1 "runs out of battery" at beat 20
./center --flaky 50 --workers 1 --queue-cap 1        # a bad planner and a tiny queue
```

A wrong argument prints the usage line. The score files are in
`data/scores/`, the target images in `data/images/` (small PGM files, in the repo).
The program prints one line like

```
RESULT beats=56 drones=64 collisions=0 speed_violations=0 mosaic_accuracy=1.000 busy=0 late=0 restarts=0 elapsed=0.09
```

Exit status: 0 = ok, 2 = a safety violation reached the stage, 3 = watchdog (the show hung, which
usually means a deadlock or a missing wake-up).

## What is given and what is yours

| File | Status |
|---|---|
| `src/swarm.c`, `protocol.c`, `line_io.c`, `planner.c`, `validator.c`, `metrics.c`, `kvpool_stub.c` | given, do not edit (the planner is the "model" that sleeps for `--think-ms`) |
| `src/queue.c` | **Task 1**: bounded blocking queue |
| `src/leader.c` — `thinker_main` | **Task 2**: ask the center for plans, handle BUSY and bad plans |
| `src/center.c` — `handle_plan_req`, `serve_request` | **Task 2**: request queue, semaphore, KV pool + mutex, PLAN/BUSY |
| `src/grid.c` — `grid_claim/release/owner` | **Task 3**: occupancy grid, one lock vs one lock per row |
| `src/leader.c` — `follower_main` | **Task 3**: the two-barrier beat |
| `src/grid.c` — `grid_swap` | **Task 4**: it deadlocks in per-row mode; find it with `swapstress`, fix it |
| `src/center.c` — `on_leader_exit` | **Task 5**: a captain goes dark: reap, restart |

If you finished Lab 6, copy your `kvpool.c` and `arena.c` into `src/`: the Makefile then uses
them instead of `kvpool_stub.c`. Your pool is not thread-safe, so wrap every call in `pool_mu`.

## Processes and threads

```
 center (process)                                    leader i (process, a child of center)
   main: clock loop (THEME, BEAT, waits READY/ACK)     main:     reads stdin (ASSIGN THEME PLAN BUSY BEAT)
   reader i (thread): reads leader i's stdout   <----  thinker:  PLAN_REQ -> validate -> READY
   workers (pool): queue -> plan_section -> PLAN  ---> follower 0..K-1 (threads): barrier, claim cells
                                                       the barrier's last thread sends POS... and ACK
```

The center and each leader talk through two pipes (the Lab 4 `fork`/`dup2`/`exec` pattern), one
text line per message. The protocol is in the project spec, Section 3.

## Debugging tips

* A hang prints `center: TIMEOUT` after `--timeout` seconds. Attach with `gdb -p <pid>` and run
  `thread apply all bt` to see who waits for what.
* `./leader` writes its log to stderr, which the center passes through.
* `valgrind --tool=helgrind ./tests/test_queue` is a good second opinion for Task 1.
* Never `sleep()` to make a race disappear.
