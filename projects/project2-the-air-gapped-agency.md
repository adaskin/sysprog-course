# Project 2: The Air-Gapped Agency — Communicating Agents via File Mailbox

**Course:** BIL 322 System Programming
**Topic:** Process orchestration, file-based IPC, status/report protocols, multi-agent coordination
**Mode:** Pairs (individual allowed; expectations are the same)
**Duration:** ~2 weeks
**AI use:** Allowed, but must be disclosed and you must be able to explain all code

---

## 1. Story

Welcome to the Agency.

The Agency runs a cluster of autonomous analysis agents — `scout` (searches
data), `analyst` (crunches it), `reporter` (writes the final briefings).
Business was good until **the Incident**: an agent with network access
decided the internet needed a copy of the archive. The firewalls stopped it.
The lawyers did not calm down for weeks.

The director's decree, effective immediately:

> **The network is gone. Physically unplugged. Agents may coordinate only
> through the shared filesystem — like operatives leaving notes in dead
> drops. And someone has to build the coordination system. That someone
> is you.**

Your deliverable: a working **orchestrator shell** plus a team of agents
that pass tasks, statuses, reports, and messages through a file mailbox —
no sockets, no network, no external APIs. Just processes, files, and
discipline.

It sounds like a limitation. It is actually how a remarkable amount of real
infrastructure works: CI systems, batch farms, and classified networks all
coordinate through filesystems. You're not building a toy; you're building
the air-gapped version of GitHub Actions with agents.

---

## 2. The Big Picture

```text
┌────────────────────┐
│ User / CLI Shell   │   you, typing commands
└─────────┬──────────┘
          │ commands
          ▼
┌────────────────────┐
│ Orchestrator       │   task table, command parser, monitor
│ (your Lab 4 code,  │
│  grown up)         │
└─────────┬──────────┘
          │ creates task files, launches agents, collects statuses
          ▼
┌────────────────────┐
│ Workspace/Mailbox  │
│   tasks/           │   task_001.task        "what to do"
│   status/          │   task_001.status      "how it's going"
│   reports/         │   task_001.report      "what came out"
│   messages/        │   msg_*.txt            "agents talking"
│   logs/            │   task_001.log         "everything it said"
└─────────┬──────────┘
          │ reads/writes files
          ▼
┌────────────────────┐
│ Agent Processes    │   scout, analyst, reporter, yours...
└────────────────────┘
```

Everything the system knows lives in files. If the orchestrator crashes and
restarts, it can rebuild its world by reading the workspace. That property —
**the filesystem is the source of truth** — is the soul of this project.

---

## 3. The Mailbox Protocol

All files are simple key-value lines. No JSON parser required (JSON is a
bonus). Every file write that others may read must be **atomic**:
write `name.tmp`, then `rename()` to the final name.

### 3.1 Task file — `tasks/task_001.task`

```text
id: task_001
agent: analyst
input: data/survey.csv
output: data/survey_summary.txt
priority: 1
```

A task file is a work order dropped in the mailbox. The orchestrator picks
it up and launches the right agent.

Note: task ids are **strings** (`task_007`), not the integers from Lab 4.
Keep them zero-padded so alphabetical order matches chronological order, and
build every derived filename directly from the id
(`status/task_007.status`, `reports/task_007.report`, …).

### 3.2 Status file — `status/task_001.status`

While running:

```text
id: task_001
agent: analyst
pid: 45678
state: running
```

When finished — **any** normal exit, nonzero codes included:

```text
id: task_001
agent: analyst
pid: 45678
state: done
exit_code: 3
report: reports/task_001.report
```

When the orchestration itself failed:

```text
id: task_001
agent: analyst
pid: 45678
state: error
message: exec failed (agent binary not found)
```

State semantics — be precise, your grade depends on it:

- `state: done` means the agent **ran and exited normally**. A nonzero exit
  code is the agent's *answer*, not a crash (remember `grep`'s exit 1).
  Read `exit_code` and the report for the verdict.
- `state: error` is reserved for **infrastructure failures**: `fork` failed,
  `exec` failed (the child `_exit(127)`s), the agent was killed by a signal,
  or `waitpid` itself failed.
- The `message` line is written by the **orchestrator**, never the agent —
  in the error case the agent may be dead and unable to write anything.

### 3.3 Report file — `reports/task_001.report`

```text
agent: analyst
input: data/survey.csv
rows_processed: 1204
status: success
```

Written by the agent, read by the orchestrator — or by **another agent**.
The filename is **derived from the task id**: task `task_007` →
`reports/task_007.report`. No configuration, no negotiation.

### 3.4 Message file — `messages/msg_001.analyst.reporter.txt`

```text
from: analyst
to: reporter
task: task_001
message: Analysis complete. Report at reports/task_001.report. Please brief.
```

Naming convention: `msg_<seq>.<from>.<to>.txt`. This is the dead drop —
agent-to-agent communication without either process ever meeting.

Sequence number rules:

- `<seq>` is a zero-padded counter (`001`, `002`, …) assigned by the
  **orchestrator** — all `send` traffic goes through it, so there is exactly
  one authority handing out numbers.
- The orchestrator must derive the next number **from the workspace** (scan
  `messages/` and take max + 1), not from memory — otherwise a restart
  reuses numbers and overwrites unread mail.
- Like every shared file, a message is published atomically: write
  `msg_001.analyst.reporter.txt.tmp`, then `rename()`.

### 3.5 Who Writes What (File Ownership)

| File | Written by | Read by |
|---|---|---|
| `tasks/*.task` | you (by hand) or the orchestrator | the orchestrator |
| `status/*.status` | **orchestrator only** | anyone |
| `reports/*.report` | **the agent only** | orchestrator, other agents |
| `messages/*.txt` | the orchestrator (`send` command) | the addressed agent |
| `logs/*.log` | the agent (via the orchestrator's `dup2`) | humans |

One rule underneath the whole table: **exactly one writer per file.** Two
writers and you're back in Act 4 with torn data.

---

## 4. Phases

Build in order. Each phase is a working system; later phases extend it.

### Phase 1: The Orchestrator (your lab, promoted)

Start from your Lab 4 shell. Required commands:

```text
run <program> [args...]
list
status <task_id>
wait <task_id>
report <task_id>
log <task_id>
exit
```

Upgrades required for the project:

- **Automatic reaping**: `list` and `status` must use
  `waitpid(pid, &status, WNOHANG)` to pick up finished agents without an
  explicit `wait`. (In the lab this was a bonus; agents that finish must
  not linger as zombies waiting for someone to notice.)
- **Atomic status writes**: tmp + `rename()`, as in the protocol.
- **Full workspace in the repo**: `tasks/`, `status/`, `reports/`,
  `messages/`, `logs/`, `data/` all ship with `.gitkeep` files, and
  `make clean` removes generated content **without** deleting the
  placeholders (same rule as the lab).

### Phase 2: The Mailbox

New commands:

```text
submit <task_file>     read a .task file, launch the named agent, track it
tasks                  list all tasks read from the workspace (not just memory)
collect <task_id>      wait (if needed), then print its report
```

Example:

```text
agency> submit tasks/task_001.task
Task submitted: task_001 (agent: analyst, pid 45678)

agency> tasks
ID         AGENT      STATE
task_001   analyst    RUNNING

agency> collect task_001
agent: analyst
rows_processed: 1204
status: success
```

🤔 For your report: `submit` translates a task file into an `execvp`
argument array. The launch convention is **fixed** so any team's agent can
run on any team's orchestrator:

```text
./agents/<agent> <task_id> <input> <output>
```

Document in your report how you build that `argv` (and what you do when a
field is missing).

Robustness requirements for `submit`:

- **Reject duplicate task ids** — if `task_007` already exists (in memory or
  in the workspace), refuse with a clear error instead of clobbering it.
- **Reject invalid task files** — missing `id`/`agent`/`input`, an agent
  binary that doesn't exist, unreadable input: print a clear error and don't
  fork. A task that can't run must never produce a zombie of confusion.
- **`collect` on a task with no report** (agent finished but wrote nothing):
  print a clear "no report produced" message. That agent broke its contract —
  say so, don't crash.

### Phase 3: The Agent Team

Implement **at least two** agents (three recommended). They must be real C
programs that read task context from their arguments, write reports, and
honor a documented exit-code convention.

Starter ideas (also fine to invent your own):

| Agent | Job |
|---|---|
| `scout` | searches input for a keyword, report lists matching lines |
| `analyst` | computes statistics over input (counts, averages) |
| `reporter` | reads another task's report, writes a final briefing |
| `archivist` | compresses/copies outputs into an archive folder |

They simulate AI agents — no ML required. What makes them "agents" is the
protocol, not the intelligence.

### Phase 4: Agents Talking to Agents

New command:

```text
send <from> <to> <task_id> <message>
```

The orchestrator writes `messages/msg_<seq>.<from>.<to>.txt`. At least one
agent must **read its mailbox** and act on it. How does an agent find its
mail? By **scanning `messages/`**: files named `msg_*.<from>.<itsname>.txt`
(or check the `to:` line inside each file). Deleting consumed messages is
allowed; leaving them is also fine — document your choice.

The canonical demo:

```text
1. submit a task for analyst
2. analyst finishes, writes reports/task_001.report
3. send analyst reporter task_001 "please brief"
4. submit a task for reporter
5. reporter finds the message, reads analyst's report,
   writes reports/task_001.briefing
```

The reporter never met the analyst. The filesystem introduced them.

### Phase 5: The Advisor (rule-based suggestions)

New command:

```text
suggest
```

The orchestrator inspects the workspace and suggests the next command:

| Situation | Suggestion |
|---|---|
| no tasks at all | `submit tasks/task_001.task` |
| tasks running | `tasks`, `status <id>` |
| a task done, report unread | `collect <id>` |
| a task failed | `log <id>`, then resubmit |
| analysis done, no briefing | `send analyst reporter ...` |

Plain `if` statements over the task table and status files. This is the
seed of "the IDE suggests what to do next" — and proof that a useful
advisor needs no LLM, just state.

---

## 5. Constraints (From the Director's Office)

1. **No networking.** No sockets, no HTTP, no curl. The wire is unplugged;
   act like it.
2. **C and POSIX only** for the core (orchestrator + agents).
3. **Communication channels allowed:** files (task/status/report/message),
   logs, exit codes. Pipes allowed only between a parent and its own child.
4. **Makefile build**, `-Wall -Wextra -Werror`, no warnings.
5. **No zombies, ever.** `ps aux | grep defunct` after any session: clean.
6. **Atomic publishes** for every file another process may read.
7. JSON, GUI, and LLM suggestions are **optional bonuses**, never required.
   An LLM advisor must respect the air gap: **local only** — a small
   offline model (e.g. llama.cpp) or a scripted mock. No network calls, no
   API keys, and the rule-based `suggest` must still work when the LLM
   component is absent entirely.

---

## 6. Milestones and Demo

Suggested schedule (2 weeks):

| Milestone | By end of | Evidence |
|---|---|---|
| M1: Phase 1+2 | week 1 | `submit`/`tasks`/`collect` work from task files |
| M2: Phase 3 | week 1 | two agents produce real reports |
| M3: Phase 4 | week 2 | the analyst→reporter dead-drop demo works |
| M4: Phase 5 + polish | week 2 | `suggest` behaves; repo clean; report written |

Demo script (what we will ask you to run, live):

```text
1. make clean && make
2. start the orchestrator
3. submit a task; show tasks/ status/ while it runs
4. collect; show the report
5. send a message; run the second agent; show the briefing
6. suggest  (twice, in different states)
7. ps aux | grep defunct   (must be empty)
8. show a status file being rewritten atomically (tmp + rename in code)
```

---

## 7. Grading

### Required (100 points)

| Item | Points |
|---|---:|
| Builds cleanly; repo organization and commit history | 10 |
| Phase 1: orchestrator (incl. automatic reaping) | 20 |
| Phase 2: mailbox protocol (`submit`/`tasks`/`collect`, atomic writes) | 20 |
| Phase 3: two working agents with reports + exit-code conventions | 15 |
| Phase 4: agent-to-agent message demo | 15 |
| Phase 5: rule-based `suggest` | 10 |
| `report.md` (design decisions, protocol docs, AI disclosure) | 10 |
| **Total** | **100** |

### Bonus (capped at +20)

| Bonus | Points |
|---|---:|
| Third agent with a distinct role | +5 |
| GUI dashboard (task table, message box, log viewer — core logic stays CLI) | +10 |
| JSON task/status support (alongside key-value) | +5 |
| Timeout/retry for stuck agents (`WNOHANG` + elapsed-time check) | +5 |
| LLM-based suggestions (local/mock only — air gap applies) | +5 |
| Priority field honored when multiple tasks are pending | +5 |

---

## 8. What We Provide vs. What You Build

**Provided:** your own Lab 4 code (the foundation), the protocol spec above,
example task/message files, and the lecture's Field Manual.

**You build:** the mailbox commands, the agents, the message flow, the
advisor, and the documentation.

---

## 9. Report Requirements

`report.md` must include:

1. Names and student IDs.
2. **Protocol documentation**: your exact file formats and naming rules
   (another team should be able to write an agent for your system using
   only this section).
3. The `submit` → `execvp` argument convention (the 🤔 from Phase 2).
4. A crash-recovery discussion: the orchestrator dies mid-session and
   restarts. Which state can it rebuild from the workspace? What is lost?
   Hints: status files carry `id`/`agent`/`pid`/`state`; `kill(pid, 0)`
   tests whether a recorded pid is still alive; a file saying
   `state: running` whose pid is dead needs a verdict; and anything kept
   only in memory (like the message counter — see §3.4) must be derivable
   from the workspace or it *will* be lost.
5. Demo output of the analyst→reporter dead drop.
6. Known bugs and limitations.
7. AI usage disclosure.

---

## 10. Final Word from the Director

> "The network people said the agents couldn't coordinate without sockets.
> Show them two processes passing notes through a directory tree and ask
> them again."

Files are enough. Make it work.
