# Lab 10: The Exam Orchestrator — Many Agents, One Server

**BIL 322 System Programming**
**Duration:** one lab session (core) + one week (homework/bonus)
**Topic:** TCP servers: threads, processes, `select`, `poll`, `epoll`; framing over a byte stream
**Starter:** [`lab10-exam-server-skeleton/`](lab10-exam-server-skeleton/)
**Lectures:** 7a/7b (threads), 4/5 (processes), 10a–10c (networking, sockets)

## Goal

An AI-agent platform has one **orchestrator** (think of an LLM that coordinates workers) and many small **agents** that connect to it. Here the orchestrator runs an exam:

```text
 agent 1001 ─┐                                    ┌─ questions.csv (answer key)
 agent 1002 ─┼── TCP ──>  orchestrator (YOU)  ────┤
   ...       │            examsrv: 5 server models ├─ students.csv (id, pin, exam)
 agent 1800 ─┘                                    └─ results/<id>.txt (one submission each)
```

Each **student agent** connects, logs in with its id and PIN, asks for its questions, submits its answers and asks for the result and corrections. **No real AI service is used:** the questions, the grading and the 1000 mock agents are deterministic, so we can concentrate on the system programming.

The grading logic is **given**. You write the part that matters for this course: how one server process waits for and serves many clients. You build the *same* orchestrator five times:

| Model | One… | Lecture idea |
|---|---|---|
| `thread` | thread per connection | 7a/7b |
| `fork` | process per connection | 4/5 |
| `select` | thread, many sockets | 10b |
| `poll` | thread, many sockets | 10b |
| `epoll` | thread, many sockets (Linux) | 10b |

By the end you should be able to:

1. Cut request lines out of a TCP byte stream (partial reads, several requests in one `recv`).
2. Write all bytes safely (partial `send`, `EINTR`, `EAGAIN`, `SIGPIPE`).
3. Serve many clients with threads, with processes, and with an event loop.
4. Explain what each model costs and when it breaks (fd limits, blocking work, slow clients).
5. Measure the models with a supplied Python load tester.

## What is given

- `exam.c` — protocol, CSV loading, grading, duplicate detection. No networking.
- `main.c` — listening socket, `SO_REUSEADDR`, signal handling (`SIGPIPE` ignored, Ctrl-C sets `g_stop`), `conn_read()`.
- `tools/gen_data.py` — seeded data (identical for everyone): 1000 agents (ids 1001–2000), 3 exams × 20 multiple-choice questions.
- `tools/loadtest.py` — conformance checks and the load test. **Read it:** it is a short asyncio *client* and a good example of the other side of the protocol.
- `Makefile`, `tools/test_mode.sh`.

## Protocol

Text, one request per line (`\n` or `\r\n`), at most 512 bytes per line. Every reply is one or more lines.

| Request | Reply |
|---|---|
| `LOGIN <id> <pin>` | `OK <name> <exam_id> <num_questions>` or `ERR BADLOGIN` / `ERR BADFORMAT` |
| `QUESTIONS` | `Q <n>\|text\|A\|B\|C\|D` for each question, then `END` (`ERR NOLOGIN` if not logged in) |
| `SUBMIT <answers>` | `OK`, `ERR BADFORMAT`, or `ERR DUPLICATE`. `<answers>` = one letter A–D per question, e.g. `ABDC…` |
| `RESULT` | `SCORE <s>/<n>`, then `WRONG <q> <yours> <correct>` lines, then `END`; or `ERR NOSUBMISSION` |
| `QUIT` | `BYE`, then the server closes the connection |
| anything else | `ERR UNKNOWN`. A line longer than 512 bytes: `ERR TOOLONG` and the server closes |

A submission is stored with `open("results/<id>.txt", O_CREAT | O_EXCL)`: the **first** submission wins, atomically, for threads *and* for processes. That is why this lab needs no mutex and no shared memory (one of your NOTES questions).

`exam.c` sleeps a few milliseconds when it grades (`EXAM_GRADE_DELAY_MS`, default 2) to imitate real work. Keep that in mind when you read your timings.

## Setup

```bash
cd labs/lab10-exam-server-skeleton
make data      # creates data/ (do not commit it)
make           # builds ./examsrv
./examsrv thread 5200 &   # once you implemented TODO 1-4
printf 'LOGIN 1001 0000\n' | nc 127.0.0.1 5200      # try it by hand with nc
```

`make test MODE=<model>` starts the server, runs the checks and a load test of 300 agents, and prints `PASS` or `FAIL`. Run `make reset` (or let `make test` do it) between runs: an agent can submit only once.

## Part A — Core (lab session)

### Task 1: Framing and writing — TODOs 1–3 in `conn.c` (25%)

TCP delivers a stream, not messages (lecture 10b). Implement:

- **TODO 1** `conn_next_line()`: take one complete request line out of the receive buffer (handle `\r\n`, a line that arrives byte by byte, several lines in one `recv`, and a line that is too long).
- **TODO 2** `send_all()`: write everything; retry on `EINTR`; on `EAGAIN` wait with `poll(POLLOUT)`; never die from `SIGPIPE` (`MSG_NOSIGNAL`).
- **TODO 3** `conn_serve()`: process every complete line in the buffer with `exam_handle_line()` and send the replies.

### Task 2: Thread per connection — TODO 4, `srv_thread.c` (25%)

Accept in a loop; give each client a detached thread with its **own** `conn_t` (never pass the address of a loop variable). Close the fd when done. After the load test the server must have the same number of open fds as before (`ls /proc/<pid>/fd | wc -l`; the tester prints it).

```bash
make test MODE=thread
```

### Task 3: One thread, `poll()` — TODO 7, `srv_poll.c` (25%)

Keep a growing array of `struct pollfd` plus the matching `conn_t*`. Make client sockets non-blocking. Close on EOF, error, `POLLHUP`, or when `conn_serve()` says so. Remove a closed client by swapping in the last entry, and be careful while iterating.

```bash
make test MODE=poll
```

### Task 4: One thread, `epoll` — TODO 8, `srv_epoll.c` (15%)

Same logic as `poll`, but with `epoll_create1`, `epoll_ctl` and `epoll_wait`, storing the `conn_t*` in `event.data.ptr`. Level-triggered is enough.

```bash
make test MODE=epoll
```

## Part B — Homework, due next lab (10%)

- **TODO 5** `srv_fork.c`: process per connection. Child closes the listener; parent closes the client fd; no zombies (`SIGCHLD`).
- **TODO 6** `srv_select.c`: one thread, `select()`. Rebuild the `fd_set` every iteration. A client with `fd >= FD_SETSIZE` must be refused, **not** crash the server.
- **`NOTES.md`** (half a page): your timing table and short answers to:
  1. Fill in the table below with the CSV lines from `make test-all` (and one run with `--clients 800 --concurrency 800` for `poll`/`epoll`). Which model was fastest, and why does the `EXAM_GRADE_DELAY_MS` delay matter?
  2. Why does this lab need no mutex? Why does `fork` still reject duplicate submissions without shared memory?
  3. `select` vs `poll` vs `epoll`: what is rebuilt or scanned on every call, and what is the fd limit?
  4. If one request takes 50 ms (`EXAM_GRADE_DELAY_MS=50`), what happens to the *other* clients in the thread server? In the epoll server?
  5. Where in your code does a partial `recv` or `send` matter?

| model | agents | seconds |
|---|---:|---:|
| thread | 300 | |
| fork | 300 | |
| select | 300 | |
| poll | 300 | |
| epoll | 300 | |

### Bonus (optional, no penalty if skipped)

- Thread **pool** instead of thread-per-connection.
- `epoll` edge-triggered (`EPOLLET`): you must read until `EAGAIN`.
- An output buffer with `EPOLLOUT`, so a slow reader cannot stall the loop.
- Hand the slow grading work of an event-loop server to a worker thread.

## What the tester checks

`tools/loadtest.py` runs: wrong/missing login, bad formats, duplicate submit (also from a new connection), requests glued together in one `send`, a request sent one byte at a time, a 3000-byte line, clients that vanish in the middle of a request, **one idle client with half a request that must not block the others**, then 300 agents (each logs in, reads questions, submits with a known number of wrong answers, checks its score). It also compares the server's open fds before and after and looks for zombie children.

## Common mistakes

- Assuming one `recv` is one request (or that a reply arrives in one `recv` on the client side).
- Passing `&client_fd` to a thread, or reusing one `conn_t` for several clients.
- Forgetting to close the client fd in the parent after `fork`, or in any model on error paths.
- Rebuilding nothing for `select` (the set is modified by the call).
- Removing an entry from the `pollfd` array while iterating without re-checking the swapped-in entry.
- Freeing a `conn_t` twice, or using it after `close`.
- Blocking in the event loop (a slow handler stalls every client).
- Writing without `MSG_NOSIGNAL` (or without ignoring `SIGPIPE`) and dying when a client disappears.

## Submission

Submit `conn.c`, `srv_thread.c`, `srv_poll.c`, `srv_epoll.c` (Part A) and `srv_fork.c`, `srv_select.c`, `NOTES.md` (Part B). Do not submit `data/`, `results/` or the binary.

| Component | Weight |
|---|---:|
| `conn.c`: framing, `send_all`, `conn_serve` | 25% |
| Thread server | 25% |
| `poll` server | 25% |
| `epoll` server | 15% |
| Homework: `fork`, `select`, `NOTES.md` | 10% |

You may use AI tools for explanation and debugging under the course policy. You must be able to explain every line you submit: each `recv`, `send`, `close`, and why the server cannot block or leak.
