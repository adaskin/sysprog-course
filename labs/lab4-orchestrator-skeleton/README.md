# Lab 4 Skeleton: Mini AI Orchestrator Shell

Starter code for BIL 322 Lab 4. See `4lab-mini-ai-orchestrator-shell.md`
for the full assignment.

## Build

```bash
make
```

## Run

```bash
./aiorch
```

## Layout

```text
include/orchestrator.h   task struct + function signatures
src/main.c               shell loop + parser (provided, do not rewrite)
src/orchestrator.c       task logic — your TODOs live here
agents/                  test agents: sleeper, failer, wordcount
data/                    sample inputs
logs/ reports/ status/   workspace dirs (kept in git via .gitkeep)
```

## Your TODOs

1. `run_command` child side: `open`/`dup2`/`close` redirection into the log.
2. `wait_for_task`: rewrite the status file after a successful wait.
3. One new agent of your own (see lab document, Task 8).
