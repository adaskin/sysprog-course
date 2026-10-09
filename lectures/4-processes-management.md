---
title: "Process Management: Building an AI Agent Orchestrator"
author: "Ammar Daskin"
marp: true
paginate: true
size: 16:10
theme: default
class: invert
---

<style type="text/css">
div {
  font-size: clamp(10px, 2vw, 28px);
  text-align: left;
}
img {
  display: block;
  width: 70%;
  text-align: center;
}
</style>

# Process Management

## Building an AI Agent Orchestrator

*Week 4*

Today we build one thing, step by step: a tiny orchestrator that launches
worker "agents", watches them, collects their reports — and we will break it
repeatedly on purpose, because every breakage teaches us one system call.

---

# 🎬 Cold Open: What Is an "Agent Framework" Really Doing?


It is 2026:

- every week there is a new agent framework,
- and they all demo the same trick:

```text
orchestrator spawns a "researcher" agent
orchestrator spawns a "coder" agent
orchestrator waits, collects the report, replies
```

Strip away the marketing, the LLM calls, the YAML. What is left?

---

# 🧅 The Onion, Peeled

```text
Agent framework
  └─ an orchestrator loop
       └─ spawns worker processes        fork
       └─ turns workers into tools        exec
       └─ waits and collects results      wait, exit status
       └─ workers leave logs/reports      open, read, write
       └─ wires outputs to inputs         dup2, pipe
```

**An AI agent orchestrator is a shell with ambition.**

> So this week we learn Unix process management the honest way: by building that shell — and watching it fail until it works.

---

# 🎭 Cast of Characters

```text
User
  ↓
Orchestrator agent        (our shell — the parent)
  ↓
Worker agents             (child processes: grep, sed, awk, our own C programs)
  ↓
Reports / status files / logs   (the "memory" of the system)
```

In this course, an **agent does not have to mean an LLM.**

---

An agent is any process the orchestrator can launch and supervise:

```text
log_analyzer     config_editor     summarizer
file_searcher    report_writer     ./worker (your C program)
```

---

# 🗂️ The Agent Workspace

All coordination happens through files. A workspace might look like:

```text
workspace/
├── logs/
│   ├── task_001.log
│   └── task_002.log
├── reports/
│   ├── task_001.report
│   └── task_002.report
├── status/
│   ├── task_001.status
│   └── task_002.status
└── data/
    └── input.txt
```
**Why files?**

---

**Why files?** 

- Processes have **isolated memory**
  - a worker cannot hand its parent a string, 
  - but it can leave a file behind, like a note on the fridge.

---

# 📨 A Status File Is a Message

A worker (or its launcher) drops this into `status/task_001.status`:

```text
task_id: task_001
agent: log_analyzer
pid: 12345
state: running
```

---

and when finished, rewrites it:

```text
task_id: task_001
agent: log_analyzer
pid: 12345
state: done
exit_code: 0
report: reports/task_001.report
```

Plain text I/O — but it is the coordination mechanism of the whole system.
Keep this picture in mind; we will come back to it.

---

# 🎛️ The Orchestrator

What is the goal?

---
🎯 **Goal**

A program that shows a prompt, reads a command, runs it. Forever.

---

# 😴 Act 0: The Lazy Orchestrator

First attempt — the laziest possible thing that could work:

```c
/* aiorch_v0.c — "it may include a few bugs..." */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    char inbuf[256] = {'\0'};
    printf("aiorch id: %d\n", getpid());
    while (1) {
        write(1, "aiorch> ", 8);
        int n = read(0, inbuf, 255);
        if (n <= 0) { perror("read"); continue; }
        inbuf[n] = '\0';        /* read() does not null-terminate! */
        system(inbuf);          /* let someone else do everything */
    }
}
```

Will this work?

---

# 😲 Act 0: It Works?!

```text
aiorch> ls
aiorch> grep ERROR logs/app.log
aiorch> ps aux | head
```

It runs everything. Even the pipeline on line 3 works!

So... are we done? Lecture over?

🤔 **What did `system()` actually do to make all of this work?**

---

# 🕳️ Act 0: Inside `system()`

```c
int system(const char *command);
```

`system()` runs your string through `/bin/sh -c`. Internally it is:

```text
fork() → child execs /bin/sh -c "your string" → parent waitpid()
```

- fork, exec, wait — hidden inside one library call. 
- That is why even `ps aux | head` works: a real shell parsed it.

> The machine works. ⚠️ We just don't control it! 
> And that has a price?

---

# 💸 Act 0: The Bill Arrives

`system()` is a black box, and the box has a price list:

- **No control**: you cannot redirect the worker's output into
  `logs/task_001.log` without shell string surgery.
- **No identity**: you don't learn the worker's PID — you can't monitor
  or kill a *specific* agent.
- **A shell in the middle**: every string is *parsed* by `/bin/sh`.

That last point is not an inconvenience. It is a security hole.

---

# 💉 Act 0: The Injection

Suppose our orchestrator politely builds a command from user input:

```c
char cmd[512];
snprintf(cmd, sizeof(cmd), "grep %s %s", user_pattern, user_file);
system(cmd);
```

A malicious "user" (or a confused LLM agent!) provides:

```text
user_pattern = "ERROR report.txt; rm -rf workspace"
```

🤔 What does `/bin/sh` do with this string?

---

# 💥 Act 0: The Injection — Answer

`/bin/sh` sees a `;` — a command separator — and happily runs **both**:

```text
grep ERROR report.txt
rm -rf workspace
```

> An orchestrator that passes strings to a shell is an orchestrator that
> executes whatever its agents hallucinate.

We will come back to this. First, let's do it properly.

---

# 🦀 Rust Callout: No Shell by Default

```rust
Command::new("grep").args([user_pattern, user_file]).status()?;
```

Rust's `Command` execs the program **directly** — arguments never touch a
shell. The injection string from the previous slide would reach `grep` as
one weird pattern, not as commands. You'd have to explicitly write
`sh -c` to get the danger back.

*(Semester note: 🦀 boxes are pattern-recognition only. You don't need to
write Rust — just notice it's the same syscalls, with guardrails.)*

---

# 🛎️ Before Act 1: How Do We Ask the Kernel for Anything?

We want to launch programs ourselves. But user programs cannot create
processes, open files, or touch hardware directly.

They **ask the kernel** via system calls:

```text
User process
   ↓ system call (trap into kernel mode)
Kernel performs the privileged operation
   ↓ returns result (+ errno on failure)
Back to user mode
```

The syscalls we need this week:

```text
open read write close      fork execve waitpid      dup2 pipe
```

---

# 🔧 One Peek Under the Hood (and only one)

On x86-64, `write(1, "Hello\n", 6)` is really:

```asm
movq $1, %rax     ; syscall number 1 = write
movq $1, %rdi     ; fd 1 = stdout
movq $msg, %rsi   ; buffer
movq $6, %rdx     ; length
syscall           ; trap into kernel
```

Different architectures, same idea (x86-32: `int $0x80`, ARM: `svc`).

Remember two things:
- syscalls are **expensive** (mode switch) — batch when you can;
- they **fail** — always check return values, read `errno`/`perror`.

(Full tables live in the Field Manual at the end.)

---

# 🗒️ Before Act 1: How Workers Leave Notes — File Descriptors

Everything an agent touches is a **file descriptor**: a small integer that
indexes into the process's table of open files.

| FD | Name | Meaning |
|---:|---|---|
| 0 | `STDIN_FILENO` | standard input |
| 1 | `STDOUT_FILENO` | standard output |
| 2 | `STDERR_FILENO` | standard error |

```c
int fd = open("logs/task_001.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
/* fd is probably 3: 0,1,2 were already taken by the terminal */
write(fd, "worker started\n", 15);
close(fd);
```

---

# 🦀 Rust Callout: Files in Rust

```rust
let mut f = File::create("logs/task_001.log")?;  // open(O_CREAT|O_WRONLY|O_TRUNC)
f.write_all(b"worker started\n")?;               // write, retried until done
drop(f);                                         // close — automatic anyway
```

Same syscalls underneath. The `?` is the errno check C lets you forget,
and `write_all` is the partial-write loop we'd have to write by hand.

---

# 📁 Why "Everything Is a File" Matters to Us

```text
logs are files
reports are files
status files are files
pipes are file descriptors
sockets are file descriptors   (next weeks!)
```

One mental model — `open / read / write / close` — covers every
communication channel our orchestrator will ever use.

*(The kernel keeps three levels: per-process fd table → system-wide open
file table → inode table. This matters in Act 4, when two processes share
one log file and things get weird.)*

---

# 🔁 Reminder: `fwrite` vs `write` (Weeks 2–3)

Same destination (an fd), two layers:

| stdio — buffered | syscall — straight to the kernel |
|---|---|
| `fread` / `fwrite` / `fprintf` | `read` / `write` |
| buffer lives in the `FILE *` (user space) | no buffer — every call is a mode switch |
| `fflush` pushes the buffer down | nothing to flush |

```text
fprintf(f, ...) → sits in libc's buffer → eventually one write(2)
```

---

Why it matters this week:

- block buffering is why agent logs look **empty until exit** — hence
  `fflush(stdout)` in long-running agents
- and why a failed-exec child calls **`_exit(127)`, not `exit(127)`** —
  `exit` would flush the *parent's* copied buffer into the log twice

---

# 🤔 If stdio Is So Convenient, Why Do We Need `read()`/`write()`?

stdio buffers precisely *because* syscalls are expensive (Act 1's peek
under the hood). But you drop to the raw layer when:

- **you only have an fd** — pipes, sockets, `dup2` targets; there is no
  `FILE *` until you `fdopen()`
- **buffering breaks the semantics** — the agent's log must be visible
  *now*; readers are polling the file
- **between `fork()` and `exec()`** — the child's stdio buffers are stale
  copies of the parent's; don't touch them
- **the operation *is* fd-level** — `dup2`, `O_APPEND`, nonblocking flags

---

⚠️ One rule above all: **never mix layers on one channel.**
`fprintf(f, ...)` and `write(fd, ...)` on the same file = the hidden stdio
buffer reorders your output. Pick one layer per stream.

---

# 🎭 Act 1: Doing It Properly — `exec`

"Fine," we say, "no more shell-in-the-middle. We'll run the program
directly."

`exec` **replaces the current process image** with a new program:

```c
#include <unistd.h>

int execl(const char *path, const char *arg, ..., NULL);
int execv(const char *path, char *const argv[]);
/* ...plus p/e variants — see Field Manual */
```

Key fact to memorize:

> **On success, `exec` never returns.** The old program is *gone* —
> code, stack, heap, all replaced. Same PID, new personality.

---

# 🚀 Act 1: Orchestrator v1

```c
/* aiorch_v1.c — "no more system()!" */
while (1) {
    write(1, "aiorch> ", 8);
    int n = read(0, inbuf, 255);
    if (n <= 0) { perror("read"); continue; }
    inbuf[n - 1] = '\0';

    if (strncmp(inbuf, "exit", 4) == 0) exit(0);

    /* crude: try as-is, then try /bin/<cmd> */
    if (execl(inbuf, inbuf, NULL) == -1) {
        char path[256];
        snprintf(path, sizeof path, "/bin/%s", inbuf);
        if (execl(path, inbuf, NULL) == -1) perror("execl");
    }
}
```

Looks reasonable. Before we run it — predict it.

---

# 🧠 Act 1: Run It in Your Head

```text
aiorch id: 4242
aiorch> ls
```

🤔 What happens next?

**A)** We see the directory listing, then the prompt returns.

**B)** We see the directory listing, and then... nothing. Ever.

**C)** Compile error.

Take a vote.

---

# 💀 Act 1: The Orchestrator Commits Suicide

```text
aiorch id: 4242
aiorch> ls
file1.c  file2.c  workspace/
...and the prompt never comes back.
```

Answer: **B**. `ls` ran beautifully — **as** our orchestrator. `exec`
replaced the orchestrator's process image with `ls`. When `ls` exited,
there was nobody left to print the next prompt.

Our orchestrator could launch exactly **one** agent, and the launch was
also its own death.

```text
aiorch> run worker   →   aiorch *becomes* worker   →   aiorch is no more
```

---

# 💡 Act 1: The Realization

We need **two** processes:

- one to *stay* (the orchestrator),
- one to *become* the worker (the agent).

Unix, unusually, gives us two separate syscalls for this:

| Syscall | Job |
|---|---|
| `fork()` | create a **copy** of the current process |
| `exec...()` | **replace** the copy with the program we actually want |

Why two instead of one "spawn"? Because in between fork and exec you can
adjust the child: redirect its fds, change its environment... That gap is
where all of Act 4 lives.

---

# 🦀 Rust Callout: Rust Bundles fork+exec

Rust's standard library has **no raw `fork`**. `Command` packages the
whole fork → adjust → exec sequence:

```rust
Command::new("ls").status()?;   // fork + exec + waitpid, one call
```

Want the raw "replace myself" semantics of Act 1? That exists too:
`std::os::unix::process::CommandExt::exec()` — and just like in C,
it never returns on success.

---

# 📑 Act 2: `fork()` — the Photocopier

```c
#include <unistd.h>
pid_t fork(void);
```

One call, **two returns**:

| Return value | Who sees it |
|---|---|
| `> 0` | parent — the value is the **child's PID** |
| `0` | child |
| `-1` | failure (check `errno`) |

The child is a near-perfect copy:

- same memory snapshot
- same open fds
- same working directory, same environment

Both continue at the instruction **after** `fork()` — parent and child alike.

```text
orchestrator = parent      worker agent = child
```

---

# 🔮 Act 2: Predict This Program

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("before fork, pid: %d\n", getpid());

    fork();                     /* no if! */

    printf("hello from pid %d (parent %d)\n", getpid(), getppid());
    return 0;
}
```

🤔 How many lines print? In what order? Who is whose parent?

---

# 👯 Act 2: Both of Them Keep Running

```text
before fork, pid: 100
hello from pid 100 (parent 42)
hello from pid 101 (parent 100)
```

**3 lines.** Everything after `fork()` runs **in both** processes.

- the order of the last two lines is **not guaranteed** — the scheduler decides
- (did you get the order right, or just a plausible one?)

Hence the universal idiom:

```c
if (pid == 0) { /* child: become the worker */ }
else          { /* parent: stay the orchestrator */ }
```

---

# 🚀 Act 2: Orchestrator v2

```c
/* aiorch_v2.c — added fork() */
pid_t child_pid = fork();

if (child_pid == 0) {           /* child: become the worker */
    if (execl(inbuf, inbuf, NULL) == -1) {
        char path[256];
        snprintf(path, sizeof path, "/bin/%s", inbuf);
        if (execl(path, inbuf, NULL) == -1) perror("execl");
    }
    _exit(127);                 /* only reached if exec failed */
} else if (child_pid > 0) {     /* parent: stay alive */
    printf("launched worker, pid %d\n", child_pid);
} else {
    perror("fork");
}
```

The prompt comes back! The orchestrator lives!

---

# 🦀 Rust Callout: `spawn()` = fork+exec Without the Wait

```rust
let child = Command::new("grep").args(["ERROR", "app.log"]).spawn()?;
// orchestrator keeps running — child.id() is the worker's PID
```

`spawn()` ≈ fork+exec and returns immediately (v2 behavior — parent stays
alive). `.status()` ≈ fork+exec+wait (v3 behavior, coming soon).

The `Child` value it returns is basically our future task-table row:
it carries the PID and knows how to `wait()`.

---

# 🔍 Act 2: It Survives! ...But Check the Process Table

Run v2, launch a few workers, then in another terminal:

```bash
ps aux | grep defunct
```

🤔 What do you expect to see?

---

# 🔢 Act 2: Quick Concept Check — Count the Processes

```c
fork();
fork();
printf("Hello from PID %d\n", getpid());
```

🤔 How many lines print?

And with **four** `fork()`s in a row?

---

# ✅ Act 2: The 2ⁿ Answer

Two forks: each `fork()` **doubles** the population — 1 → 2 → 4
processes. So **4 lines**.

Four forks: \(2^4 = 16\) processes, 16 lines.

General rule: `n` forks → \(2^n\) processes (if everyone forks).

The PIDs in the output look random — again, the *scheduler* decides who
runs when. Never assume parent-before-child or child-before-parent.

---

# 💣 Act 2: The Fork Bomb

```c
while (1) fork();
```

**Do not run this.**

🤔 Trace it in your head: what does this do to the machine in one second?
In ten?

---

# 🤖 Act 2: The Fork Bomb, or: the Agent That Spawns Agents

Each copy makes copies: exponential growth until the process table is
full and the machine can't even run `ps` to complain.

Why mention this in an AI course? Because the modern version is:

```text
agent decides to "delegate"
  → spawns sub-agent
      → sub-agent decides to "delegate"
          → ...
```

An orchestrator without a spawn limit is a fork bomb with a business plan.

Defenses:

- `ulimit -u` and cgroups on the OS side
- in our design: a fixed-size task table with `MAX_TASKS`

---

# 🧟 Act 3: The Report Nobody Collected

Back to our `ps` experiment — the answer:

```text
$ ps aux | grep defunct
ammar   4247 ... [ls] <defunct>
ammar   4249 ... [grep] <defunct>
```

A finished child is not fully gone. Until the parent **collects** it, the
kernel keeps a small tombstone:

- PID
- exit status
- resource usage

> **Zombie: an agent that finished, but nobody ever picked up its report.**

- enough zombies → the process table fills → no new forks
- a long-running orchestrator that never reaps is a memory leak with PIDs

---

# ❓ Act 3: Two Problems, One Question

The zombie tombstone tells us two things are missing from v2:

1. How do we **remove** the tombstone (free the PID)?
2. The worker's `main` returned a value — its *final word* on whether
   the task succeeded. **Where did that number go?** How can the
   orchestrator read it?

🤔 Guess: one syscall solves both. What would you call it?

---

# ⚰️ Act 3: Reaping with `wait` / `waitpid`

```c
#include <sys/wait.h>

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
```

```c
int status;
pid_t done = waitpid(child_pid, &status, 0);   /* block until child exits */
```

One call, two jobs:

1. **Reaps** the zombie (frees the kernel's tombstone),
2. **Delivers** the exit status into `status` — the worker's final word.

That returned `main` value? It was sitting in the tombstone all along.

---

# 📜 Act 3: Exit Status — Reading the Final Word

`status` is a packed integer; read it with macros:

| Macro | Question it answers |
|---|---|
| `WIFEXITED(status)` | did it exit normally? |
| `WEXITSTATUS(status)` | if so, with what exit code? |
| `WIFSIGNALED(status)` | was it killed by a signal? |
| `WTERMSIG(status)` | if so, by which one? |

```c
if (WIFEXITED(status)) {
    printf("agent exited with code %d\n", WEXITSTATUS(status));
} else if (WIFSIGNALED(status)) {
    printf("agent killed by signal %d\n", WTERMSIG(status));
}
```

---

# 🃏 Act 3: A Trick Question

Orchestrator launches a `log_searcher` agent (really: `grep`) to find
errors in a log. `grep` finishes with exit code **1**.

🤔 Did the agent fail? Take a vote: 👍 failed / 👎 succeeded.

---

# 🃏 Act 3: The Answer Is in the Manual

`man grep`:

| Exit code | `grep` means |
|---:|---|
| 0 | match found |
| 1 | no match |
| 2 | actual error |

---

# ✅ Act 3: No Match Is Not Failure

Answer: **no** — exit code 1 from `grep` means "I did my job; there was
nothing to find." The log is clean. That's a *result*, not a crash.

```c
if (WIFEXITED(status)) {
    int code = WEXITSTATUS(status);
    if (code == 0)      printf("matches found\n");
    else if (code == 1) printf("no matches — log is clean\n");
    else                printf("grep error\n");
}
```

> Nonzero does not always mean failure. An orchestrator that treats
> "no match" as "agent crashed" will make bad decisions.

Convention for our own agents: `0` success, small codes = conditions,
`127` = "exec failed" (we set it via `_exit(127)`), signals = killed.

---

# ⏳ Act 3: Blocking vs. Non-Blocking

`waitpid(pid, &status, 0)` **blocks**:

- the orchestrator freezes until the worker finishes
- sometimes that's what you want (`wait <task_id>`)

But a dashboard-style orchestrator wants to *peek*:

```c
int rc = waitpid(task->pid, &status, WNOHANG);
if (rc == 0) {
    /* still running — do something else, check again later */
} else if (rc > 0) {
    /* finished: reap + read status */
} else {
    perror("waitpid");
}
```

This is how an agent monitor keeps its prompt responsive while workers
churn in the background. (`waitpid(-1, &status, WNOHANG)` peeks at *any*
child.)

---

# 🦀 Rust Callout: `wait()` and `try_wait()`

```rust
let mut child = Command::new("worker").spawn()?;

let status = child.wait()?;    // waitpid(pid, &status, 0) — blocks

match child.try_wait()? {      // waitpid(pid, &status, WNOHANG)
    Some(status) => println!("done: {status}"),
    None => println!("still running"),
}
```

Notice the `Option` again: "no news yet" is a real value (`None`),
not a magic `0` return code you have to remember.

---

# 👪 Act 3: Orchestrator v3 — a Responsible Parent

```c
pid_t child_pid = fork();

if (child_pid == 0) {
    execvp(args[0], args);
    perror("exec failed");
    _exit(127);
} else if (child_pid > 0) {
    int status;
    waitpid(child_pid, &status, 0);          /* reap */
    if (WIFEXITED(status))
        printf("worker done, exit code %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("worker killed by signal %d\n", WTERMSIG(status));
} else {
    perror("fork");
}
```

The orchestrator now launches agents, survives, and collects their final
words. Are we done?

---

# 📣 Act 4: But the Workers Are Shouting

v3 works, but watch the terminal: every worker's `printf` lands on **our**
screen, mixed with the orchestrator's own prompts.

An orchestrator wants each agent's voice filed away:
`logs/task_001.log`, `logs/task_002.log`, ...

🤔 We cannot edit the worker's source code — it might be `grep`, a binary
we didn't write. How do we send *its* output into *our* file?

Hint: remember which table survives `exec`.

---

# 🔀 Act 4: Sending the Worker's Voice to a File

In shell syntax we want:

```bash
./worker_agent input.txt > logs/task_001.log 2>&1
```

The shell pulls this off with `dup2`:

```c
int dup2(int oldfd, int newfd);
```

- `dup2` makes `newfd` point to the **same open file description** as `oldfd`
- so `dup2(logfd, STDOUT_FILENO)` re-wires "fd 1" to our log file
- and the fd table **survives `exec`** — the worker never knows its stdout
  became a file

That was the hint: the fd table is exactly the part of us that `exec`
doesn't replace.

---

# 🎯 Act 4: Redirecting an Agent's stdout+stderr

In the child, between `fork` and `exec` (the gap from Act 1!):

```c
int logfd = open("logs/task_001.log",
                 O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (logfd < 0) _exit(126);

dup2(logfd, STDOUT_FILENO);    /* stdout → log file */
dup2(logfd, STDERR_FILENO);    /* stderr → same log file */
close(logfd);                  /* fd 3 no longer needed */

execvp(program, argv);
perror("exec failed");
_exit(127);
```

Now `worker_agent`'s `printf`s and its error messages both land in the
log. The orchestrator's terminal stays clean. This *is* what
`> file 2>&1` compiles down to.

---

# 🦀 Rust Callout: Redirection Without `dup2`

```rust
let log = File::create("logs/task_001.log")?;
Command::new("worker_agent").arg("input.txt")
    .stdout(Stdio::from(log.try_clone()?))   // dup2(logfd, 1)
    .stderr(Stdio::from(log))                // dup2(logfd, 2)
    .status()?;
```

The fork/exec gap where we did `dup2` by hand is exactly where
`.stdout()` / `.stderr()` hook in — same fd rewiring, done for us.

---

# 🔮 Act 4: Predict This One

```c
int fd = open("data.txt", O_RDONLY);   /* before fork */
pid_t pid = fork();                    /* child inherits fd 3 */

if (pid == 0) {
    read(fd, buf, 10);                 /* child reads first     */
} else {
    wait(NULL);
    read(fd, buf, 10);                 /* then parent reads     */
}
```

`data.txt` contains `AAAABBBBCCCCDDDD...` (20+ bytes).

🤔 Which bytes does the **parent** get: `AAAA...` again, or `BBBB...`?

---

# 🤯 Act 4: Why Shared Descriptors Surprise You

Answer: the parent gets the **next** 10 bytes — the child moved the offset
for both of them.

Why? Remember the kernel's three levels:

```text
per-process fd table → open file table (shared!) → inode
```

The **file offset lives in the open file table**, not in the fd. After
`fork`, both fd tables point at the **same** open file description — one
offset, shared. The child advances it for the parent.

---

# 🔁 Act 4: Now the Flip Side

```c
pid_t pid = fork();
if (pid == 0) {
    int fd = open("data.txt", O_RDONLY);   /* opened AFTER fork */
    read(fd, buf, 10);
} else {
    int fd = open("data.txt", O_RDONLY);   /* opened AFTER fork */
    wait(NULL);
    read(fd, buf, 10);
}
```

🤔 Now what does the parent read — same first 10 bytes, or the next 10?

---

# 📏 Act 4: Two Agents, One Log — the Rule

Answer: **both read the same first 10 bytes.** Each `open()` after the
fork creates its own open file table entry — its own offset.

**Rule of thumb for agents:**

| Situation | Offsets |
|---|---|
| fd inherited across `fork()` | **shared** |
| each process opens separately | **independent** |
| many agents append to one log | open with `O_APPEND` — every write jumps to the end atomically |

---

# ✂️ Act 4: The Half-Written Status File

- the orchestrator polls `status/task_001.status` every second
- the worker writes it with three `fprintf` calls

🤔 What might the orchestrator read if it opens the file **between** the
worker's `fprintf`s?

---

# ✂️ Act 4: A Torn File

It can read a **half-written** file:

```text
state: done
exi
```

Half a status is worse than none: `state: done`, no exit code — did the
agent succeed? Unparseable garbage?

---

# 🛡️ Act 4: Don't Publish Half a File — `rename`

The fix — write-then-rename:

```c
FILE *f = fopen("status/task_001.status.tmp", "w");
fprintf(f, "state: done\nexit_code: 0\n");
fflush(f);                    // stdio → kernel (fclose does this too, but be explicit)
fsync(fileno(f));             // ask the kernel to flush to the device;
fclose(f);                    // release the fd

rename("status/task_001.status.tmp", "status/task_001.status");   
```

`rename()` within one filesystem is **atomic**: readers see either the
old file or the new one, never a mixture.

Same reason the careful version of `sed` is
`sed ... > tmp && mv tmp file`, not a blind `sed -i`.

---

# 🦀 Rust Callout: Atomic Rename

```rust
let mut f = OpenOptions::new().create(true).write(true).truncate(true)
    .open("status/task_001.status.tmp")?;
f.write_all(b"state: done\n")?;
f.sync_all()?;                        // ← the line that's easy to forget
fs::rename("status/task_001.status.tmp", "status/task_001.status")?;   
```

Same `rename` syscall, same atomicity guarantee — the pattern is the
lesson, not the language.

---

# 🗣️ Act 5: Agents Talking to Agents

The orchestrator's dream:

```bash
grep ERROR logs/app.log | awk '{print $3}' > reports/errors.txt
```

Two worker processes, one's stdout feeding the other's stdin, the final
result filed into a report.

🤔 What *is* the `|` character, physically? How do bytes get from one
process's stdout into another's stdin — through a file? Through memory?
Through the kernel?

---

# 🚰 Act 5: Pipes

Answer: a kernel buffer with two file descriptors.

```c
#include <unistd.h>
int pipe(int pipefd[2]);   /* pipefd[0] = read end, pipefd[1] = write end */
```

A pipe is just two fds the kernel connects with a buffer. It is
**not seekable**, and it follows one sacred rule:

> **Close every end you don't use.** A reader blocks forever if *any*
> process holds the write end open; only all-writers-closed produces EOF.

Keep this rule in mind — we're about to see why.

---

# 🛠️ Act 5: Building `grep | awk` by Hand

Plan first (always draw this on the board):

```text
create pipe
fork → child1: stdout → pipe write end → exec grep
fork → child2: stdin → pipe read end, stdout → report file → exec awk
parent: close both pipe ends, wait for both
```

🤔 Before we write it: why does the **parent** need to close the pipe
ends? It never reads or writes them.

---

# 1️⃣ Act 5: Stage One — `grep`

```c
int p[2];
if (pipe(p) < 0) { perror("pipe"); return -1; }

pid_t left = fork();
if (left == 0) {
    close(p[0]);                     /* we never read */

    dup2(p[1], STDOUT_FILENO);       /* stdout → pipe */
    close(p[1]);

    execlp("grep", "grep", "ERROR", "logs/app.log", (char *)NULL);
    perror("exec grep failed");
    _exit(127);
}
```

---

# 2️⃣ Act 5: Stage Two — `awk`

```c
pid_t right = fork();
if (right == 0) {
    close(p[1]);                     /* we never write */

    dup2(p[0], STDIN_FILENO);        /* stdin ← pipe */
    close(p[0]);

    int out = open("reports/errors.txt",
                   O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) _exit(126);
    dup2(out, STDOUT_FILENO);
    close(out);

    execlp("awk", "awk", "{print $3}", (char *)NULL);
    perror("exec awk failed");
    _exit(127);
}
```

---

# 🐛 Act 5: Parent Cleanup — Spot the Bug

Version A (a student wrote this):

```c
/* parent */
int s1, s2;
waitpid(left, &s1, 0);
waitpid(right, &s2, 0);
```

Version B:

```c
/* parent */
close(p[0]);
close(p[1]);

int s1, s2;
waitpid(left, &s1, 0);
waitpid(right, &s2, 0);
```

🤔 One of these hangs forever. Which, and why?

---

# 🥶 Act 5: The Hang, Explained

Version **A** hangs.

- the parent kept `p[1]` open → *someone* still holds the write end,
  even after `grep` exits
- so `awk` never sees EOF, never finishes
- and `waitpid(right, ...)` waits forever

The orchestrator freezes, students reboot, lesson learned:

> EOF happens only when **every** write end, in **every** process, is
> closed. Including the parent's.

This is inter-process communication built from three fds and discipline.
You've now manually done what `|` does in every shell.

---

# 🦀 Rust Callout: `grep | awk` in Rust

```rust
let mut grep = Command::new("grep").args(["ERROR", "logs/app.log"])
    .stdout(Stdio::piped())                          // pipe(p) + dup2(p[1], 1)
    .spawn()?;

let awk = Command::new("awk").arg("{print $3}")
    .stdin(Stdio::from(grep.stdout.take().unwrap())) // dup2(p[0], 0)
    .stdout(File::create("reports/errors.txt")?)
    .spawn()?;
```

Rust closes the parent's pipe ends for you when the handles drop — the
Version-A hang from two slides ago is much harder to write by accident.

---

# 🏗️ Act 6: The Orchestrator Itself

Everything we survived assembles into the real thing — a REPL:

```text
aiorch> run grep -n ERROR logs/app.log
launched task 1 (pid 4247)
aiorch> run awk -F: '{print $2}' data/input.txt
launched task 2 (pid 4249)
aiorch> list
aiorch> status 1
aiorch> wait 1
task 1 done, exit code 0
aiorch> report 1
aiorch> log 1
aiorch> exit
```

Still a shell. But every command now maps to one thing we built today.

---

# 📋 Act 6: The Task Table — Our Fork-Bomb Shield

```c
#define MAX_TASKS 64

typedef enum { TASK_EMPTY, TASK_RUNNING, TASK_DONE, TASK_ERROR } TaskState;

typedef struct {
    int id;
    pid_t pid;
    TaskState state;
    int exit_code;
    char command[256];
} Task;
```

Bounded on purpose: no table slot, no fork. (Remember Act 2's bomb.)

---

# 👶 Act 6: `run` — the Child Side

```c
int run_command(Task *tasks, int *count, char **argv) {
    if (*count >= MAX_TASKS) {
        fprintf(stderr, "task table full\n");
        return -1;
    }
    char logpath[256];
    snprintf(logpath, sizeof logpath, "logs/task_%03d.log", *count + 1);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {                       /* child: become the agent */
        int logfd = open(logpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd < 0) _exit(126);
        dup2(logfd, STDOUT_FILENO);
        dup2(logfd, STDERR_FILENO);
        close(logfd);
        execvp(argv[0], argv);
        perror("exec failed");
        _exit(127);
    }
    /* parent side: next slide */
```

---

# 🧑 Act 6: `run` — the Parent Side

```c
    /* parent: register the agent */
    Task *t = &tasks[*count];
    t->id = *count + 1;
    t->pid = pid;
    t->state = TASK_RUNNING;
    /* (note: store the full command line, not just argv[0]) */
    snprintf(t->command, sizeof t->command, "%s", argv[0]);
    (*count)++;

    /* publish the status file atomically */
    char tmp[256], fin[256];
    snprintf(tmp, sizeof tmp, "status/task_%03d.status.tmp", t->id);
    snprintf(fin, sizeof fin, "status/task_%03d.status", t->id);
    FILE *f = fopen(tmp, "w");
    fprintf(f, "task_id: %d\npid: %d\nstate: running\n", t->id, pid);
    fclose(f);
    rename(tmp, fin);

    printf("launched task %d (pid %d)\n", t->id, pid);
    return 0;
}
```

---

# ⏱️ Act 6: `wait <id>` and `status <id>`

```c
int wait_for_task(Task *tasks, int count, int id) {
    Task *t = find_task(tasks, count, id);
    if (!t) return -1;

    int status;
    if (waitpid(t->pid, &status, 0) < 0) {
        perror("waitpid");
        t->state = TASK_ERROR;
        return -1;
    }
    if (WIFEXITED(status)) {
        t->exit_code = WEXITSTATUS(status);
        t->state = TASK_DONE;
    } else {
        t->state = TASK_ERROR;
    }
    /* rewrite status/<id>.status atomically (tmp + rename) */
    return 0;
}
```

And `status <id>` is the same with `WNOHANG` — peek, don't freeze.

---

# 🧟 The Failure Taxonomy: Zombies (Recap)

Two undead species every orchestrator must know. First, the zombie:

```text
child exits → parent never calls wait → kernel keeps the tombstone
```

Cure:

- `waitpid(..., 0)` when you can block
- periodic `waitpid(-1, &st, WNOHANG)` sweeps when you can't

Live demo: run `zombie_example.c`, then `ps aux | grep defunct`.

> A zombie is an agent that finished but whose report was never collected.

---

# 🥺 The Failure Taxonomy: Orphans — Predict

```c
if (fork() == 0) {
    sleep(5);   /* parent exits during this sleep */
    printf("PID=%d, new PPID=%d\n", getpid(), getppid());
}
```

The parent exits immediately; the child sleeps 5 more seconds.

🤔 When the child finally prints, what is its parent PID? Who is raising
this child now?

---

# 🏠 The Failure Taxonomy: Orphans — Answer

```text
PID=1234, new PPID=1
```

Orphans are **adopted by PID 1** (`init`/`systemd`), which reaps them
when they finally exit.

Real-world reading: your agent framework crashes, its workers keep
burning CPU/GPU — parentless, unsupervised, still billing you.

> An orphan is an agent abandoned by a crashed orchestrator.

Design question for Project 2: how would your orchestrator's status files
help a *restarted* orchestrator rediscover its surviving workers?

---

# ⚖️ Why Processes At All? (vs Threads)

| Processes | Threads |
|---|---|
| separate memory | shared memory |
| one crash stays local | one bug can kill all |
| costlier context switch | cheaper |
| communicate via files/pipes/status | communicate via shared variables |

For untrusted, flaky, third-party workers (read: anything an LLM emitted),
**isolation wins**:

- that's why agent runtimes sandbox workers as processes
- and why next month's threads lecture will feel dangerous

---

# 🦀 Rust Corner: The Same Story, Civilized

We've met 🦀 callouts all along the way; here is the whole story in one
place. Rust's standard library is this lecture with a seatbelt.

**C (what we just did):**
```c
pid_t pid = fork();
if (pid == 0) { execlp("grep", "grep", "-n", "ERROR", "app.log", NULL); _exit(127); }
waitpid(pid, &status, 0);
```

**Rust:**
```rust
let status = Command::new("grep")
    .args(["-n", "ERROR", "app.log"])
    .status()?;              // fork + exec + waitpid, inside
```

`Command` ≈ our `run_command`: builder for spawn, fds, env, then reap.

---

# 🦀 Rust Corner: `ExitStatus` vs `wait` Macros

Same packed kernel status, friendlier accessors:

| C | Rust |
|---|---|
| `WIFEXITED(status)` | `status.code().is_some()` / `success()` |
| `WEXITSTATUS(status)` | `status.code()` → `Option<i32>` |
| `WIFSIGNALED(status)` | `status.signal()` → `Option<i32>` (unix ext) |

```rust
if output.status.success() {
    println!("matches found");
} else if output.status.code() == Some(1) {
    println!("no matches");          // grep's "condition", not failure
} else {
    println!("grep error");
}
```

🤔 Why does `code()` return `Option<i32>` instead of `i32`?

---

# 🦀 Rust Corner: Why `Option`?

Because **there might be no exit code** — a process killed by a signal
never produced one.

```rust
match output.status.code() {
    Some(0) => println!("ok"),
    Some(1) => println!("no match"),
    Some(c) => println!("error {c}"),
    None    => println!("killed by signal — no exit code exists"),
}
```

C lets you call `WEXITSTATUS` without checking `WIFEXITED` first — and
hands you garbage. Rust makes the "maybe" part of the type.

---

# 🦀 Rust Corner: `Result` vs `errno`

C's error channel is a **global variable** set as a side effect:

```c
int fd = open("nope", O_RDONLY);
if (fd == -1) perror("open");        /* reads errno behind the curtain */
```

Rust makes the error a **return value you cannot ignore**:

```rust
let mut f = File::open("nope")?;     // Err(...) must be handled or `?`-ed
```

| C | Rust |
|---|---|
| return `-1`, set `errno` | return `Result<T, E>` |
| forgetting to check: silent corruption | forgetting: **compile error** (must_use) |
| `perror` / `strerror(errno)` | `err.to_string()`, `?` propagation |

Same kernel failures underneath. Different discipline at the call site.

---

# 🦀 Rust Corner: The Rest of the Parallels

**Redirecting to a log** (our Act 4 `dup2` dance):
```rust
let log = File::create("logs/task_001.log")?;
Command::new("worker_agent").arg("input.txt")
    .stdout(Stdio::from(log.try_clone()?))
    .stderr(Stdio::from(log))
    .status()?;
```

**Atomic status write** (our rename pattern):
```rust
fs::write("status/task_001.status.tmp", "state: done\n")?;
fs::rename("status/task_001.status.tmp", "status/task_001.status")?;
```

The OS concepts are identical; only the guardrails moved.

---

# 🔒 Security Rules for Orchestrators (I)

1. **No `system()` on untrusted input.** Ever. Agent output is untrusted.
2. Use `execvp`/`execv` with an **argument array** — no shell parsing:

   ```c
   char *argv[] = {"grep", user_pattern, user_file, NULL};
   execvp(argv[0], argv);
   ```

3. Never concatenate user/agent strings into shell commands.
4. Check **every** return value (`fork`, `open`, `dup2`, `waitpid`...).

---

# 🔒 Security Rules for Orchestrators (II)

5. `_exit(127)` after a failed `exec` in the child (not `return` —
   you don't want to run the parent's cleanup handlers twice).
6. Close every fd you don't need — especially pipe ends.
7. Redirect stdout **and** stderr into per-task logs.
8. Treat exit codes as meaningful agent state, not just pass/fail.

Pin these next to your lab keyboard.

---

# 🧪 Lab: Mini AI Orchestrator Shell

This lecture's v0→v3 arc *is* the lab specification. In C, non-GUI,
using `fork`/`exec`/`wait` — no `system()`:

```text
aiorch> run <program> [args...]     launch agent, log to logs/task_<id>.log
aiorch> list                        all tasks with state
aiorch> status <task_id>            peek with WNOHANG
aiorch> wait <task_id>              block, reap, update status file
aiorch> report <task_id>            print reports/task_<id>.report
aiorch> log <task_id>               print logs/task_<id>.log
aiorch> exit
```

Status files written **atomically** (tmp + rename). Exit codes interpreted
(grep semantics!). No zombies left behind: `ps aux | grep defunct` is the
grader's first command.

---

# 📬 Project 2: Communicating Agents via File Mailbox

The orchestrator grows up:

- multiple named agents, running concurrently,
- **task / status / report** files per agent (this week),
- **message files** between agents: `mailbox/<agent>.inbox`,
- the orchestrator suggests the next command based on reports
  (rules-based is fine; `grep`+`awk` over status files is fine).

Optional extras: JSON messages, a GUI dashboard, LLM-based suggestions.

Everything it needs already exists in today's Acts — the project is
composition, not new syscalls. (Sockets come later; files are enough.)

---

# 🏁 Summary: The Story So Far

We wanted an orchestrator. It kept dying, so we kept learning:

| Breakage | Fix | Lesson |
|---|---|---|
| `system()` = black box + injection | do it ourselves | shell parsing is dangerous |
| v1 died after one command | `fork()` | exec replaces; fork copies |
| runaway spawning | task table + limits | fork bomb = uncontrolled agents |
| `<defunct>` everywhere | `wait`/`waitpid` | reap your agents |
| "no match" looked like failure | `WEXITSTATUS` semantics | exit codes are messages |
| workers shouting on our terminal | `dup2` | redirection is fd surgery |
| half-read status files | tmp + `rename` | atomic publication |
| piping between agents | `pipe` + close discipline | EOF needs all writers closed |

---

# ⏭️ Next Week

Memory management — the memory side of agent infrastructure:

- what exactly gets copied at `fork()` (copy-on-write!)
- heap layout, `malloc`/`free` internals
- fragmentation, alignment
- building a small allocator

---
---

# 📖 Field Manual (Reference Appendix)

*Everything below is lookup material — for homework, labs, and exams.
We won't narrate it in class; consult it like a man page.*

---

# 📎 FM 1: Syscall Entry Instructions by Architecture

| Architecture | Instruction | Syscall # Register | Return Registers |
|---|---|---|---|
| x86-32 | `int $0x80` | `eax` | `eax`, `edx` |
| x86-64 | `syscall` | `rax` | `rax`, `rdx` |
| ARM (EABI) | `svc 0x0` | `r7` | `r0`, `r1` |
| MIPS | `syscall` | `v0` | `v0`, `v1`, `a3` |

`man 2 syscall`, and the full list: `man 2 syscalls`.

Direct invocation without a wrapper:

```c
#include <sys/syscall.h>
syscall(SYS_write, 1, "Hello\n", 6);
```

---

# 📎 FM 2: Traps vs Exceptions vs Interrupts

| Term | Kind | Examples |
|---|---|---|
| Trap | intentional, synchronous | syscalls, breakpoints |
| Exception | error, synchronous | div-by-zero, page fault |
| Interrupt | external, asynchronous | keyboard, timer |

Man page sections: **`man 2 open`** = syscall, **`man 3 printf`** =
C library function. `printf` buffers and eventually calls `write`.

---

# 📎 FM 3: `open()` Flags and Modes

```c
int open(const char *path, int oflags);
int open(const char *path, int oflags, mode_t mode);  /* with O_CREAT */
```

| Flag | Meaning |
|---|---|
| `O_RDONLY` / `O_WRONLY` / `O_RDWR` | access mode |
| `O_APPEND` | every write goes to end (multi-agent-safe logs) |
| `O_TRUNC` | truncate to zero on open |
| `O_CREAT` | create if missing (needs `mode`, e.g. `0644`) |
| `O_EXCL` | with `O_CREAT`: fail if file exists (lock-file trick) |

Combine with `|`. Permissions: `S_IRUSR|S_IWUSR` == `0600`, etc.

---

# 📎 FM 4: `fopen()` vs `open()`

| `fopen()` (libc, man 3) | `open()` (syscall, man 2) |
|---|---|
| buffered, returns `FILE *` | unbuffered, returns fd |
| modes `"r" "w" "a" "r+" ...` | flags `O_RDONLY`, ... |
| good for parsing reports (`fgets`) | needed for `dup2`/pipes/redirection |

Rule of thumb: parse reports with `fopen`/`fgets`; redirect agent output
with `open`/`dup2`.

---

# 📎 FM 5: `fopen` Mode → `open` Flags

| `fopen` mode | `open` flags |
|---|---|
| `"r"` | `O_RDONLY` |
| `"w"` | `O_WRONLY \| O_CREAT \| O_TRUNC` |
| `"a"` | `O_WRONLY \| O_CREAT \| O_APPEND` |
| `"r+"` | `O_RDWR` |
| `"w+"` | `O_RDWR \| O_CREAT \| O_TRUNC` |
| `"a+"` | `O_RDWR \| O_CREAT \| O_APPEND` |

---

# 📎 FM 6: `read()` / `write()` Contract

```c
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t len);
```

- `read` returns bytes actually read; `0` = EOF; `-1` = error.
- `write` may write **fewer** bytes than asked (partial write) — loop.
- Pipes/sockets may return partial **reads** too.
- Null-terminate before printing: `buf[n] = '\0';`
- Always check the return value. Always.

---

# 📎 FM 7: Metadata and Seeking

```c
int stat(const char *path, struct stat *buf);   /* follows symlinks */
int fstat(int fd, struct stat *buf);            /* via open fd */
int lstat(const char *path, struct stat *buf);  /* the link itself */

off_t lseek(int fd, off_t offset, int whence);
/* SEEK_SET, SEEK_CUR, SEEK_END */
```

Useful `struct stat` fields: `st_size`, `st_mode` (with `S_ISREG()`...).
Pipes and sockets are **not seekable** — `lseek` fails on them.

---

# 📎 FM 8: The `exec` Family

| Function | Args | PATH search | Environment |
|---|---|---|---|
| `execl` | list | no | inherited |
| `execv` | array | no | inherited |
| `execlp` | list | yes | inherited |
| `execvp` | array | yes | inherited |
| `execle` | list | no | custom `envp` |
| `execve` | array | no | custom (the real syscall) |

All are wrappers around `execve`. Argument arrays **must end in NULL**.

---

# 📎 FM 9: `waitpid` Cheat Sheet

```c
pid_t waitpid(pid_t pid, int *status, int options);
```

| `pid` value | Waits for |
|---|---|
| `> 0` | that specific child |
| `-1` | any child |
| `0` | any child in same process group |

| Option | Effect |
|---|---|
| `0` | block |
| `WNOHANG` | return immediately (0 = still running) |
| `WUNTRACED` | also report stopped children |

---

# 📎 FM 10: Process Memory Layout (Recap)

| Segment | Contents |
|---|---|
| text | machine code (read-only) |
| data | initialized globals/statics |
| bss | zero-initialized globals/statics |
| heap | `malloc` country (next week!) |
| stack | locals, frames |

Plus per-process: fds, environment, PID/PPID, signal handlers.
`fork()` copies all of it (lazily, via copy-on-write — next week);
`exec()` replaces text/data/bss/heap/stack but **keeps the fd table** —
which is exactly why Act 4's redirection works.

---

*Field Manual ends. Remember: stories teach, manuals remind.*
