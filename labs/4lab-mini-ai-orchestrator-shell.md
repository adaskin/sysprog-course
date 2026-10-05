# Lab 4: Mini AI Orchestrator Shell

**Course:** BIL 322 System Programming
**Topic:** `fork` / `exec` / `waitpid`, exit statuses, file descriptors, `dup2` redirection, status/log files
**Mode:** Individual or pair work
**Deadline:** Extended — you have several days to complete this
**AI use:** Allowed, but must be disclosed and you must be able to explain all code

---

## 🛡️ Secure C Programming Rules

The standard BIL 322 secure-C rules apply (no `gets`, no unbounded `scanf %s`, prefer `snprintf`, check every return value, compile with `-Wall -Wextra -Werror`). One new rule joins the list this week:

11. **No `system()`.** Ever. In this lab you are replacing it — using it would be hiring the thing you were asked to build.

---

## 1. Story

Last week, a demo went wrong at a small AI tooling company. Their "agent framework" — a thin wrapper around `system()` — cheerfully executed a hallucinated command from one of its own agents and deleted a day's worth of experiment logs. (Remember Act 0 of the lecture. The string `ERROR report.txt; rm -rf workspace` was involved.)

The team lead's reaction:

> "No more shell-in-the-middle. We launch agents ourselves, we know their PIDs, we collect their exit codes, and every agent's output goes into its own log file. By Friday."

You are the infrastructure engineer. The framework team needs a **minimal orchestrator shell** — no Docker, no systemd, no libraries beyond libc and POSIX. In lecture you watched `aiorch` v0→v3 die and recover; now you build the real one.

### Why this matters beyond this course

| Real-world system | Connection to this lab |
| --- | --- |
| Agent frameworks (AutoGPT-style loops) | An orchestrator spawning and supervising worker processes |
| CI runners (GitHub Actions, GitLab CI) | Job = child process, log capture, exit-code verdicts |
| `bash` itself | Your `run`/`wait`/`list` are what a shell does all day |
| Process supervisors (supervisord, systemd units) | Status tracking, reaping, restart decisions |

---

## 2. What You Are Building

An interactive, non-GUI shell in C. Every command it launches is treated as an **agent process**:

```text
help
run <program> [args...]
list
status <task_id>
wait <task_id>
report <task_id>
log <task_id>
exit
```

A real session (your output should look like this):

```text
aiorch> run ./agents/wordcount data/input.txt
Started task 1
PID: 5675

aiorch> wait 1
Task 1 finished with exit code 0.

aiorch> run grep -n ERROR data/app.log
Started task 2
PID: 5677

aiorch> wait 2
Task 2 finished with exit code 0

aiorch> log 2
3:ERROR disk quota exceeded on /tmp
5:ERROR connection refused by upstream

aiorch> run ./agents/failer
Started task 3
PID: 5678

aiorch> wait 3
Task 3 finished with exit code 3

aiorch> list
ID   PID        STATE      COMMAND
1    5675       DONE       ./agents/wordcount data/input.txt
2    5677       DONE       grep -n ERROR data/app.log
3    5678       DONE       ./agents/failer
```

---

## 3. Repository Layout

The template repository (GitHub Classroom) contains:

```text
lab-orchestrator/
├── Makefile
├── README.md
├── include/
│   └── orchestrator.h
├── src/
│   ├── main.c                # shell loop + parser (provided)
│   └── orchestrator.c        # task logic (partially TODO)
├── agents/
│   ├── sleeper.c             # sleeps N seconds (provided)
│   ├── failer.c              # exits with code 3 (provided)
│   └── wordcount.c           # counts words/lines (provided)
├── data/
│   ├── input.txt
│   └── app.log
├── logs/                     # agent stdout/stderr land here
├── reports/                  # agent reports live here
└── status/                   # per-task status files live here
```

⚠️ **`logs/`, `reports/`, `status/` contain only `.gitkeep` files.** Git does
not track empty directories — without those placeholder files, cloning the
template would give you no workspace, and every agent launch would fail with
`open: No such file or directory`. If you ever reorganize, keep the `.gitkeep`
files. (The orchestrator must run from the repo root so the relative paths work.)

---

## 4. The Status File — Your Agent's Fridge Note

When `run` launches a task, the parent writes `status/task_001.status`:

```text
task_id: 1
pid: 5675
state: running
```

When the task finishes (your job, Task 5):

```text
task_id: 1
pid: 5675
state: done
exit_code: 0
```

Simple key-value lines. **No JSON parser required** — `fgets` + `strncmp` +
`sscanf` are enough.

Conventions to keep everywhere (you will need them again in Project 2):

- **Zero-padded filenames:** task 7 → `status/task_007.status`,
  `logs/task_007.log`. Build them with
  `snprintf(path, sizeof path, "status/task_%03d.status", id)`.
- **The status file is written at launch with `state: running`** and only
  refreshed when somebody waits (Task 5). A file that says `running` does not
  prove the process is alive — polling a file is not reaping.

🤔 Why write this file at all, when the parent already tracks state in its
task table? (Hint: what survives if the orchestrator crashes? This question
returns in Project 2.)

---

## 5. What Is Provided vs. What You Implement

**Provided (read them, don't rewrite them):**

- `main.c` — the REPL: prompt → `fgets` → `strtok_r` tokenization → dispatch. Note the bounds: `MAX_LINE 1024`, `MAX_ARGS 16`.
- `orchestrator.h` — `Task` struct, `TaskState` enum, function signatures.
- `orchestrator.c` — `task_add`, `task_find`, `list_tasks`, `print_task_status`, `print_task_report`, `print_task_log` are complete. `run_command` has the parent side complete and the **child side TODO**. `wait_for_task` works but leaves the status file stale.
- The three test agents and a Makefile that builds everything.

**You implement:**

| Task | What | Where |
| --- | --- | --- |
| 2 | Log redirection in the child (`open`/`dup2`/`close`) | `run_command` TODO |
| 5 | Status-file update after `wait` | `wait_for_task` TODO |
| 8 | A new agent of your own | `agents/` |

Plus testing, understanding, and the report.

---

## 6. Tasks

### Task 0: Build and break (warm-up)

```bash
make
./aiorch
```

Run a few things **before** implementing anything:

```text
aiorch> run ./agents/wordcount data/input.txt
aiorch> run ls
aiorch> exit
```

🤔 Everything "works", but where did the output go? Why is `logs/task_001.log`
missing? Explain in your report — this is the lecture's Act 4 problem, and
Task 2 is its cure.

Also try from another terminal while a `sleeper 10` task runs:

```bash
ps aux | grep defunct
```

🤔 After `sleeper` finishes but before you `wait` on it — what do you see?
That is Act 3, live.

---

### Task 1: Understand the parser (provided)

Read `parse_line` in `main.c`. Answer in your report:

1. Why `strtok_r` and not `strtok`?
2. What happens if the user types 20 arguments? (Look at `MAX_ARGS - 1`.)
3. Why does `run_command` receive `&argv[1]` instead of `argv`?

No code to write here — but the grader may ask these questions.

---

### Task 2: `run` — give the agent its log (the core TODO)

Inside `run_command` in `orchestrator.c`, the child currently goes straight to
`execvp`. Fill in the TODO:

1. `open(logpath, O_WRONLY | O_CREAT | O_TRUNC, 0644)` — `logpath` is already
computed for you (`logs/task_%03d.log`).
2. `dup2` the log fd onto **both** `STDOUT_FILENO` and `STDERR_FILENO`.
3. `close` the original fd.
4. On any failure: `_exit(126)` (our convention: 126 = "couldn't set up",
127 = "exec failed").

Then verify:

```text
aiorch> run ./agents/wordcount data/input.txt
aiorch> wait 1
aiorch> log 1
words: 25
lines: 3
```

🤔 The skeleton computes `logpath` **before** `fork()`. Why not inside the
child after fork? (Both work here — but which memory does the child have?)

---

### Task 3: `list` (provided — verify it)

Launch two tasks quickly, one long (`sleeper 5`), one instant. Run `list`
immediately. You should see one `RUNNING`, one `DONE`... or two `RUNNING`?

🤔 `list` reads only the in-memory table, and the table only changes when you
`wait`. So a finished-but-not-awaited task shows as... what? (The bonus fixes
this. Name the phenomenon first.)

---

### Task 4: `status` (provided — verify it)

```text
aiorch> status 1
Task ID: 1
PID: 5675
Command: ./agents/wordcount data/input.txt
State: DONE
Exit code: 0
```

---

### Task 5: `wait` — collect the final word, update the file

`wait_for_task` already blocks with `waitpid(pid, &status, 0)` and updates the
in-memory table. Your job: **after** a successful wait, rewrite
`status/task_%03d.status` with the final state:

```text
task_id: 1
pid: 5675
state: done
exit_code: 0
```

or, for the failer:

```text
task_id: 3
pid: 5678
state: done
exit_code: 3
```

🤔 Exit code 3 — is task 3 an ERROR? The skeleton's `WIFEXITED` branch says
DONE for **any** normal exit. Defend that choice in your report. (Hint: what
does `grep` return when it finds nothing? Should a clean "no match" mark your
agent as broken?)

---

### Task 6: `report` (provided)

Agents in this lab write to stdout → captured into logs. `report` reads
`reports/task_%03d.report` — which no provided agent creates yet. That is
intentional: it prints `No report available for task N`.

In **Task 8** your own agent will produce a report, and `report` becomes
useful. (In Project 2, reports become the main communication channel.)

---

### Task 7: The zombie audit (required evidence)

Before submitting, run this experiment and paste the results into your report:

```bash
# Terminal 1
./aiorch
aiorch> run ./agents/sleeper 30

# Terminal 2, after ~35 seconds, BEFORE typing wait in terminal 1
ps aux | grep defunct

# Now in terminal 1:
aiorch> wait 1

# Terminal 2 again:
ps aux | grep defunct
```

Expected: one `<defunct>` sleeper appears, then disappears after `wait`.
**Your submission must leave no zombies**: exiting the shell after proper
waits, `ps aux | grep defunct` must show nothing from your aiorch.

---

### Task 8: Your own agent

Write one new agent in `agents/`, add it to the Makefile. Requirements:

- reads input from a file or stdin,
- writes its main output to **stdout** (which lands in the log — you get
redirection for free),
- **also** writes a real report to `reports/task_<id>.report`,
- uses a **meaningful exit-code convention** (document it).

How does the agent know its task id? The user passes it as an argument:
`run ./agents/myagent <task_id> <input>`. Task ids are sequential
(1, 2, 3, …) — check `list` or the last "Started task N" if unsure. The
agent then writes `reports/task_%03d.report` for that id (zero-padded, same
convention as the status files).

Ideas: `filter` (lines containing a keyword), `upper` (uppercase a file),
`hasher` (simple checksum), `headlines` (first N lines as a "summary").

---

## 7. Optional / Bonus Features

| Bonus | Description | Points |
| --- | --- | ---: |
| Automatic reaping in `list` and `status` | Sweep with `waitpid(pid, &status, WNOHANG)` inside **both** commands, so finished tasks show DONE (with exit code) without an explicit `wait` | +5 |
| Atomic status writes | Write `status/task_001.status.tmp`, then `rename()` — no torn reads | +5 |
| `runall <file>` | Read commands from a file, launch them all | +5 |
| `waitall` | Wait for every RUNNING task | +5 |
| Second custom agent | Another agent, different behavior | +5 |

Bonus is capped at **+15**.

Note: automatic reaping stops being a bonus in Project 2 — a multi-agent
orchestrator that needs a manual `wait` per agent is unusable, so it becomes
required there. Implementing it now saves you later.

---

## 8. Common Pitfalls (Learned the Hard Way)

1. **Empty output in the log until the agent ends.** When stdout is a file,
libc switches to block buffering — the agent's `printf` sits in a buffer.
Long-running agents should `fflush(stdout)` after progress messages
(see `sleeper.c`).
2. **`exit()` vs `_exit()` in the child after a failed exec.** `exit()` runs
the parent's atexit handlers and flushes **copies** of the parent's
buffers — your log gets duplicated lines. Use `_exit(127)`.
3. **`open` failing with "No such file or directory"** even though `logs/`
exists in your repo → you cloned without `.gitkeep` files, or you're
running `aiorch` from the wrong directory.
4. **`waitpid` returns -1 with `ECHILD`** → you're waiting on a task that was
already reaped (e.g., a WNOHANG bonus already collected it). Track state.
5. **Forgetting `close(logfd)`** after `dup2` — fd leaks matter when
MAX_TASKS grows.

---

## 9. Compilation

```bash
make          # builds aiorch + agents
make clean    # removes binaries AND generated logs/status/reports
```

⚠️ `make clean` must remove generated files **but keep the `.gitkeep`
placeholders**. The provided Makefile deletes `logs/*.log`,
`status/*.status`, `status/*.tmp`, `reports/*.report` — which leaves
`.gitkeep` untouched. If you edit the clean rule, keep it that way, or the
next `git clone` of your repo breaks (see Pitfall 3).

Flags (already in the Makefile):

```bash
gcc -Wall -Wextra -Werror -std=gnu11 -g -Iinclude
```

(`-std=gnu11` because `strtok_r`, `fork`, `dup2`, `waitpid` are POSIX, not
ISO C. Equivalently `-std=c11` with `#define _POSIX_C_SOURCE 200809L`.)

---

## 10. Grading

### Required (100 points)

| Item | Points |
| --- | ---: |
| Builds cleanly with `make` (no warnings) | 10 |
| Git repository organization and commit history | 10 |
| Task 2: log redirection (`open`/`dup2`/`close`) correct | 20 |
| Task 5: `wait` + status macros + status-file update | 20 |
| `list` / `status` verified against live tasks | 10 |
| Task 7: zombie audit evidence in report | 10 |
| Task 8: custom agent with report + exit-code convention | 10 |
| `report.md` (incl. the 🤔 questions and AI disclosure) | 10 |
| **Total** | **100** |

### Bonus (capped at +15)

See Section 7.

---

## 11. Report Requirements

Create `report.md` with:

1. Your name and student ID.
2. Answers to all six 🤔 questions and the three Task 1 parser questions:
   - Section 4: why write a status file at all, when the parent has a task table?
   - Task 0 (two): where did the output go? what does `ps` show after `sleeper` finishes but before `wait`?
   - Task 1: why `strtok_r` and not `strtok`? what happens with 20+ arguments? why does `run_command` receive `&argv[1]`?
   - Task 2: why is `logpath` computed **before** `fork()`?
   - Task 3: what does a finished-but-not-awaited task show as in `list`, and what is that phenomenon called?
   - Task 5: why does the skeleton mark **any** normal exit (even code 3) as DONE — and is that right?
3. The zombie audit (Task 7) with actual terminal output.
4. Your agent's exit-code convention, and why you chose it.
5. Known bugs or limitations.
6. AI usage disclosure.

Example AI disclosure:

```text
I used an AI assistant to understand why dup2 must happen between fork
and exec, and to debug why my logs were empty (buffering). All final
code was written, tested, and understood by me.
```

---

## 12. AI Use Policy

AI use is **allowed**. You may use AI for explaining syscalls, debugging,
and compiler errors. You must **understand**, **test**, and **disclose**.
You must not submit code you cannot explain — the lab interview will ask.

---

## 13. Submission

Push to your GitHub Classroom repository.

```text
Makefile
include/orchestrator.h
src/main.c
src/orchestrator.c
agents/  (provided three + yours)
data/
logs/.gitkeep  reports/.gitkeep  status/.gitkeep
report.md
```

Final check before submission:

```bash
make clean && make
./aiorch          # full session: run, wait, log, status, list, exit
ps aux | grep defunct    # nothing of yours
```

---

## 14. What's Next

This shell is Phase 1 of **Project 2: The Air-Gapped Agency**. Keep your
code clean and modular — you will extend exactly this orchestrator with a
file mailbox, multiple cooperating agents, and a suggestion engine.