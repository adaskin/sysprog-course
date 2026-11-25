# Lab 10: The Exam Orchestrator — Many Agents, One Server

**BIL 322 System Programming**
**Duration:** one lab session (core) + one week (homework/bonus)
**Topic:** TCP client/server programming; serving many clients with threads, processes, `poll`, `epoll`, `select`; framing over a byte stream
**Starter:** [`lab10-exam-skeleton/`](lab10-exam-skeleton/)
**Lectures:** 7a/7b (threads), 4/5 (processes), 10a–10c (networking, sockets)

## Goal

An AI-agent platform has one **orchestrator** (think of an LLM that coordinates workers) and many small **agents** that connect to it. Here the orchestrator runs an exam, and the agents are students:

```text
 student 1001 ─┐                                       ┌─ data/questions.csv (answer key)
 student 1002 ─┼──── TCP ────>   exam server  ─────────┤
     ...       │          (ONE program, bin/examsrv)   ├─ data/students.csv (id, pin, exam)
 student 1300 ─┘   MANY copies of bin/student          └─ results/<id>.txt (one submission each)
```

You write **two programs**, in two separate folders:

| Folder | Program | Instances |
|---|---|---|
| `exam-server/` | the **exam server** `bin/examsrv` | one |
| `student/` | the **student** client `bin/student` | many, started at the same time |

A student connects, logs in with its id and PIN, asks for its questions, "thinks" for a moment, submits its answers, asks for the result and corrections, and leaves. The server must serve **all** students at once. First you make one student talk to one server; then you change *how the server waits for its clients* five different ways.

**No real AI service is used:** questions, grading and the 1000 mock students are deterministic, so we concentrate on system programming. The exam logic (`exam.c`) is given.

By the end you should be able to:

1. Write a TCP **client** and a TCP **server** that speak a line-based protocol.
2. Cut lines out of a TCP byte stream (partial reads, several requests in one `recv`) and write all bytes safely.
3. Explain why a one-client-at-a-time server fails with many clients.
4. Serve many clients with threads, with processes, and with an event loop (`poll`, `epoll`, `select`).
5. Explain what each model costs and when it breaks (fd limits, blocking work, slow clients).

## What is given

- `common/proto.h`, `common/linebuf.h`: constants and the shared interface.
- `exam-server/exam.c`: protocol, CSV loading, grading, duplicate detection. No networking.
- `exam-server/main.c`: listening socket, `SO_REUSEADDR`, signals (`SIGPIPE` ignored, Ctrl-C sets `g_stop`), `conn_read()`.
- `student/student.c`: argument parsing and `main()`; the dialogue is yours.
- `tools/gen_data.py`: seeded data, identical for everyone (1000 students, ids 1001–2000; 3 exams × 20 questions).
- `tools/run_students.sh N`: starts **N copies of `bin/student` at the same time** and reports how many succeeded and how long it took.
- `tools/check_server.py`, `tools/test_server.sh`: protocol checks and a full test per server model.

## Protocol

Text, one request per line (`\n` or `\r\n`), at most 512 bytes per line. A reply is one or more lines; it is finished when you read a line that is `END` or starts with `OK`, `ERR` or `BYE`.

| Request | Reply |
|---|---|
| `LOGIN <id> <pin>` | `OK <name> <exam_id> <num_questions>` or `ERR BADLOGIN` / `ERR BADFORMAT` |
| `QUESTIONS` | `Q <n>\|text\|A\|B\|C\|D` for each question, then `END` (`ERR NOLOGIN` if not logged in) |
| `SUBMIT <answers>` | `OK`, `ERR BADFORMAT`, or `ERR DUPLICATE`. `<answers>` = one letter A–D per question |
| `RESULT` | `SCORE <s>/<n>`, then `WRONG <q> <yours> <correct>` lines, then `END`; or `ERR NOSUBMISSION` |
| `QUIT` | `BYE`, then the server closes the connection |
| anything else | `ERR UNKNOWN`. A line longer than 512 bytes: `ERR TOOLONG` and the server closes |

A submission is stored with `open("results/<id>.txt", O_CREAT | O_EXCL)`: the **first** submission wins, atomically, for threads *and* for processes. That is why this lab needs no mutex and no shared memory (a NOTES question). `exam.c` also sleeps a few milliseconds when it grades (`EXAM_GRADE_DELAY_MS`, default 2) to imitate real work.

## Setup

```bash
cd labs/lab10-exam-skeleton
make data        # creates data/ (do not commit it)
make             # builds bin/examsrv and bin/student (the stubs compile; the programs do nothing yet)
```

Try the protocol by hand once your server runs: `printf 'LOGIN 1001 <pin>\nQUIT\n' | nc 127.0.0.1 5200` (pins are in `data/students.csv`).

## Part A — Core (lab session)

### Task 1: One student, one server (35%)

Write the shared framing, the first server and the student, then connect them.

1. **TODO 1–2** `common/linebuf.c`: `lb_next_line()` (cut one complete line out of the receive buffer; handle `\r\n`, a line arriving byte by byte, several lines in one `recv`, too-long lines) and `send_all()` (partial `send`, `EINTR`, `EAGAIN` + `poll(POLLOUT)`, `MSG_NOSIGNAL`). Both programs use this file.
2. **TODO 3** `exam-server/conn.c`: `conn_serve()` processes every complete line in the buffer with `exam_handle_line()` and sends the replies.
3. **TODO 4** `exam-server/srv_iterative.c`: the simplest server. `accept` a client, serve it until it leaves, `accept` the next one.
4. **TODO S1–S3** `student/student.c`: `connect_to()`, `read_reply()` and `run_session()` (LOGIN, QUESTIONS, think, SUBMIT, RESULT, QUIT). Answer however you like.

```bash
bin/examsrv iterative 5200 &
PIN=$(awk -F, '$1==1001 {print $3}' data/students.csv)
bin/student 127.0.0.1 5200 1001 $PIN       # prints: student 1001: score 4/20
```

### Task 2: Many students, one iterative server (5%)

```bash
make reset
sh tools/run_students.sh 20 5200           # 20 student programs at once
```

How long does it take? Each student thinks for 100 ms, and the server waits for it. What do the other 19 students do in the meantime? Write the answer in `NOTES.md`. This is the problem the rest of the lab solves.

### Task 3: Thread per connection — TODO 5, `srv_thread.c` (15%)

Accept in a loop; give each client a detached thread with its **own** `conn_t` (never pass the address of a loop variable). Close the fd when done.

```bash
make test MODE=thread       # server checks + 300 students at once
```

### Task 4: One thread, `poll()` — TODO 6, `srv_poll.c` (15%)

A growing array of `struct pollfd` plus the matching `conn_t*`; non-blocking client sockets; close on EOF, error, `POLLHUP`, or when `conn_serve()` says so. Remove a closed client by swapping in the last entry, and be careful while iterating.

```bash
make test MODE=poll
```

### Task 5: One thread, `epoll` — TODO 7, `srv_epoll.c` (15%)

The same logic with `epoll_create1`, `epoll_ctl`, `epoll_wait`; store the `conn_t*` in `event.data.ptr`. Level-triggered is enough.

```bash
make test MODE=epoll
```

## Part B — Homework, due next lab (15%)

- **TODO 8** `srv_fork.c`: process per connection. The child closes the listener; the parent closes the client fd; no zombies (`SIGCHLD`).
- **TODO 9** `srv_select.c`: one thread, `select()`. Rebuild the `fd_set` every iteration. A client with `fd >= FD_SETSIZE` must be refused, **not** crash the server.
- **`NOTES.md`** (half a page): the timing table below (use `make test-all`, and a run with `sh tools/run_students.sh 800 PORT` against `poll` and `epoll`; raise `ulimit -n`) and short answers to:
  1. In Task 2, how long did 20 students take against the iterative server, and why? Which model was fastest later, and why does the grading delay matter?
  2. Why does this lab need no mutex? Why does `fork` still reject duplicate submissions without shared memory?
  3. `select` vs `poll` vs `epoll`: what is rebuilt or scanned on every call, and what is the fd limit?
  4. If one request takes 50 ms (`EXAM_GRADE_DELAY_MS=50`), what happens to the *other* students in the thread server? In the epoll server?
  5. Where in your code does a partial `recv` or `send` matter?

| model | students | seconds |
|---|---:|---:|
| iterative | 20 | |
| thread | 300 | |
| fork | 300 | |
| select | 300 | |
| poll | 300 | |
| epoll | 300 | |

### Bonus (optional)

- A thread **pool** instead of thread-per-connection.
- `epoll` edge-triggered (`EPOLLET`): you must read until `EAGAIN`.
- An output buffer with `EPOLLOUT`, so a slow reader cannot stall the loop.
- Hand slow grading work of an event-loop server to a worker thread.
- A student that is clever: parse the questions and answer the arithmetic ones correctly.

## What the tests check

`make test MODE=<model>` starts the server and runs `tools/check_server.py`: wrong/missing login, bad formats, duplicate submit (also from a new connection), requests glued together in one `send`, a request sent one byte at a time, a 3000-byte line, clients that vanish in the middle of a request, and **one idle client with half a request that must not block the others** (the iterative server is exempt). Then it starts 300 copies of **your** `bin/student` at once (20 for the iterative server) and finally compares the server's open fds before and after and looks for zombie children.

## Common mistakes

- Assuming one `recv` is one request, or one reply (in the student too!).
- Passing `&client_fd` to a thread, or reusing one `conn_t` for several clients.
- Forgetting to close the client fd in the parent after `fork`, or on any error path.
- Not rebuilding the `fd_set` for `select` (the call modifies it).
- Removing an entry from the `pollfd` array while iterating without re-checking the swapped-in entry.
- Freeing a `conn_t` twice, or using it after `close`.
- Blocking in the event loop (a slow handler stalls every client).
- Writing without `MSG_NOSIGNAL` (or ignoring `SIGPIPE`) and dying when a peer disappears.

## Submission

Submit `common/linebuf.c`, `exam-server/conn.c`, `exam-server/srv_*.c`, `student/student.c` and `NOTES.md`. Do not submit `bin/`, `data/` or `results/`.

| Component | Weight |
|---|---:|
| Task 1: framing, `conn_serve`, iterative server, student client | 35% |
| Task 2: many students against the iterative server (observation in NOTES) | 5% |
| Thread server | 15% |
| `poll` server | 15% |
| `epoll` server | 15% |
| Homework: `fork`, `select`, `NOTES.md` | 15% |

You may use AI tools for explanation and debugging under the course policy. You must be able to explain every line you submit: each `connect`, `accept`, `recv`, `send` and `close`, and why the server cannot block or leak.
