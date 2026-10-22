# Lab 7 Skeleton: Worker Agents as Threads

Starter code for BIL 322 Lab 7. See `7lab-threads-intro.md` (one folder up) for the full assignment.

## Build and run

```bash
make            # builds everything
make data       # generates the benchmark input data/big.txt (default 64 MB; SIZE_MB=200 make data)
make test       # runs the checks in tests/run_tests.sh
make tsan       # ThreadSanitizer builds of tokencount and race_fixed
```

`data/big.txt` is **generated, never committed**: `data/` is in `.gitignore`. Do not remove that rule.

## Layout

```text
common.h             timing helper, CHECK() macro for pthread calls (given)
gen_data.c           generates data/big.txt (given)
agents_bug.c         Task 1a: the &i bug, on purpose (given, do not fix)
agents.c             Task 1b, 1c: YOUR CODE
tokencount.c         Task 2: YOUR CODE (read_file is given)
creation_bench.c     Task 2: threads vs processes, YOUR CODE
race.c               Task 3: two races on purpose (given, do not fix)
race_fixed.c         Task 3: your lock-free fix, YOUR CODE
tok_race.c           Task 4: strtok vs strtok_r, YOUR CODE
swarm.c              Bonus: a swarm without locks, YOUR CODE
tests/               small inputs and run_tests.sh (given)
run_tsan.sh          runs the ThreadSanitizer builds (given)
```

Every TODO is marked `TODO` in the source. The skeleton compiles as-is; the checks fail until
you implement the tasks.

## Rules reminder

- No mutexes, semaphores, atomics, or `volatile` in this lab (they are next week). `make test`
  greps for them in the files that must stay lock-free.
- Compile warnings are errors in spirit: fix them.
- Follow the secure C rules (`../secure-c-programming-rules-for-assignments.md`).
