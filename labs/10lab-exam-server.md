# Lab 10: The Exam Server — One Client, Then Many

**BIL 322 System Programming**  
**Duration:** one lab session (core) + homework  
**Topic:** TCP clients and servers, framing, threads, processes, `select`, `poll`, and `epoll`  
**Starter:** [`lab10-exam-skeleton/`](lab10-exam-skeleton/)

## Goal

The exam server is a mock AI/orchestrator service. Each student agent is a separate TCP client. The questions and grading are deterministic; no real AI service is used.

```text
 student 1001 ─┐                                      ┌─ data/questions.csv
 student 1002 ─┼──── TCP ────>  one exam server  ─────┼─ data/students.csv
     ...       │          bin/examsrv                  └─ results/<id>.txt
 student 2000 ─┘   many copies of bin/student
```

You implement two programs:

| Program | Instances | Your work |
|---|---:|---|
| `bin/student` | Many clients | Connect, send requests, and complete one exam |
| `bin/examsrv` | One server | Accept connections and serve them using different models |

The application logic stays the same. Only the server's concurrency model changes.

## Start with one connection

Before writing any concurrent server, make **one student client** talk to **one iterative server**:

```text
 student client                         exam server
      |                                      |
      | socket(); connect()                  | socket(); bind(); listen()  [given]
      |------------ TCP connection --------->| accept()
      |                                      | conn_init()
      | LOGIN 1001 1234\n                    |
      |------------------------------------->| conn_read(); conn_serve()
      |                                      | exam_handle_line()  [given]
      |                 OK Agent0001 0 20\n  |
      |<-------------------------------------|
      | QUESTIONS / SUBMIT / RESULT / QUIT   | same connection
      |------------------------------------->|
      |                                      | close(client_fd)
```

`accept()` returns a **new socket for this client**. The listening socket remains available for later connections. This first working connection is the foundation for every later task.

## What is given, and what is yours?

### Given — do not modify

- `exam-server/main.c`: command-line handling, listening socket, common helpers, and cleanup.
- `exam-server/exam.c`, `exam.h`: CSV loading, login, questions, grading, and result storage.
- `protocol/linebuf.c`: `lb_init()`, `lb_read()`, and `lb_next_line()` framing.
- `exam-server/servers.c`: `conn_serve()` protocol dispatch.
- `student/student.c`: `read_reply()` reply collection.
- `tools/`: deterministic data, protocol checks, and concurrent student runner.

The given parsing and formatting code uses bounded buffers and checked numeric conversions. Invalid ranges, numeric overflow, overlong lines, and formatting truncation are rejected rather than silently accepted.

### Given infrastructure, not assessed in this lab

Signals have not been covered yet. Therefore `main.c` already handles:

- `SIGINT`/`SIGTERM`: sets `g_stop` so server loops can shut down;
- `SIGPIPE`: closed peers do not terminate the whole server;
- `SIGCHLD`: children in the `fork` server are reaped automatically.

You do **not** write a signal handler in this lab. Use `send_all()` with `MSG_NOSIGNAL` as shown in the comments.

### Your files

- `protocol/linebuf.c`: TODO 1, reliable sending with `send_all()`.
- `student/student.c`: TODO S1–S2, connect and complete one client session.
- `exam-server/servers.c`: TODO 2–7, all six server models.

All server functions are in **one sectioned file**. `server.h` is the only server interface header.

## Protocol and TCP framing

Requests and replies are text lines ending in `\n` or `\r\n`. The maximum request line is 512 bytes including its newline.

| Request | Reply |
|---|---|
| `LOGIN <id> <pin>` | `OK <name> <exam_id> <num_questions>` or an `ERR ...` line |
| `QUESTIONS` | `Q ...` lines, then `END` |
| `SUBMIT <answers>` | `OK`, `ERR BADFORMAT`, or `ERR DUPLICATE` |
| `RESULT` | `SCORE <s>/<n>`, optional `WRONG ...` lines, then `END` |
| `QUIT` | `BYE`, then the server closes |
| anything else | `ERR UNKNOWN` |

A reply ends at `END`, or at a line beginning with `OK`, `ERR`, or `BYE`.

**TCP does not preserve your sends as messages.** The given line buffer converts its byte stream back into requests:

```text
client sends:       "LOGIN 1001 1234\nQUESTIONS\n"

possible recv #1:   "LOG"
possible recv #2:   "IN 1001 1234\nQUES"
possible recv #3:   "TIONS\n"

line buffer:        [partial bytes until '\n' arrives]
                         |
                         +--> complete line: "LOGIN 1001 1234"
                         +--> keep leftover: "QUESTIONS" until its '\n' arrives
```

One `recv()` can therefore contain half a request, one request, or several requests. The buffer must retain an incomplete line and any requests after the line just consumed. `send()` can also write fewer bytes than requested.

For your own request formatting, use bounded `snprintf()` and check its return value whenever the returned length drives `send_all()`. Do not use `atoi()` for numeric reply fields: use the given checked `parse_int()` helper.

## Setup

```bash
cd labs/lab10-exam-skeleton
make data        # generate the same mock students and questions for everyone
make             # the stubs compile
```

Use two terminals for the first manual test:

```bash
# Terminal 1
bin/examsrv iterative 5200

# Terminal 2
PIN=$(awk -F, '$1==1001 {print $3}' data/students.csv)
bin/student 127.0.0.1 5200 1001 "$PIN"
```

The server creates `results/` automatically. Use `make reset` when you want to clear old submissions and run the same students again.

# Core lab

## Task 1: Reliable sending, one server connection, and one client (37%)

### 1a. Reliable sending — TODO 1

In `protocol/linebuf.c`, implement `send_all()`:

- Loop until every byte is sent.
- Handle partial sends and retry after `EINTR`.
- On nonblocking `EAGAIN`/`EWOULDBLOCK`, wait for `POLLOUT` with `poll()` and continue.
- Use `MSG_NOSIGNAL`.

The framing functions are given. Read `lb_next_line()` and `conn_serve()` to see how they process every complete line already buffered without losing a partial line or later messages.

### 1b. One accepted server connection — TODO 2

In `run_iterative()` in `exam-server/servers.c`:

- Call `accept()` and then the given `serve_client(client_fd)`.
- Only after that client finishes may you accept the next client.
- Retry `accept()` after `EINTR` unless `g_stop` is set.
- A client failure must not stop the listener.

The given `serve_client(fd)` owns the read loop and closes the descriptor. The given `conn_serve()` dispatches requests and sends replies. At this stage, a second client waits until the first client finishes.

### 1c. One student client — TODO S1–S2

In `student/student.c` implement:

1. `connect_to()`: `socket`, IPv4 address/port setup, `connect`, and error handling.
2. `run_session()`: `LOGIN`, `QUESTIONS`, think, `SUBMIT`, `RESULT`, and `QUIT`.

`read_reply()` is given. It collects lines until the reply terminator while preserving bytes belonging to later replies. Answer every question with `A` if you want; parsing individual questions is optional. The final output line must be:

```text
student <id>: score <s>/<n>
```

Check your first complete version:

```bash
make test MODE=iterative
```

## Task 2: Observe the iterative bottleneck (5%)

```bash
make reset
sh tools/run_students.sh 20 5200
```

Each student thinks for 100 ms after receiving its questions. While the iterative server waits for that client's next request, the other clients cannot be served. Record the elapsed time and explain it in one or two sentences in `NOTES.md`.

## Task 3: Thread per connection — TODO 3 (15%)

In `run_thread()`:

```text
                         accept loop (main thread)
                                   |
                 +-----------------+-----------------+
                 |                 |                 |
          new heap arg (fd)  new heap arg (fd)  new heap arg (fd)
                 |                 |                 |
          worker thread A    worker thread B    worker thread C
          serve_client(fd)   serve_client(fd)   serve_client(fd)
          can block          can block          can block
```

- Allocate a separate heap argument for every accepted client.
- Start a detached thread whose argument owns that client fd.
- The worker calls `serve_client(fd)` and frees its own argument.
- Never pass the address of an `accept`-loop variable to a thread.

One worker may wait for its client while the others continue.

```bash
make test MODE=thread
```

## Task 4: One thread with `poll()` — TODO 4 (15%)

`poll` uses a different approach: **one thread watches many socket descriptors**.

```text
        listener fd + client fd A + client fd B + client fd C
                               |
                               v
                      poll(..., 1000 ms)
                               |
                     only descriptors now ready
                               |
              +----------------+----------------+
              |                                 |
       listener readable?                  client readable?
       accept new client              conn_read + conn_serve
              |                                 |
              +------------- repeat ------------+
```

- Keep a dynamic `struct pollfd` array and a parallel `conn_t *` array.
- Index 0 is the listener. Accepted client sockets must be nonblocking; use the given `set_nonblocking(fd)`.
- On readable input, call `conn_read()` and then `conn_serve()` when bytes arrive.
- Close and free on EOF, a hard error, or when `conn_serve()` requests closure.
- When removing an array entry by swapping in the last entry, process that slot again.

```bash
make test MODE=poll
```

# Homework

## Task 5: `select()` — TODO 5 (7%)

Implement the same readiness loop using `select()`. Unlike `poll`, the descriptor set is both input and output:

```text
 each iteration                              next iteration
 +------------------+                      +------------------+
 | FD_ZERO(&set)    |                      | rebuild from zero |
 | FD_SET(listener) |     ready fds only   | FD_SET(listener)  |
 | FD_SET(each fd)  |  +-----------------> | FD_SET(each fd)   |
 | reset timeout    |  |                   | reset timeout     |
 +--------+---------+  |                   +------------------+
          |            |
          v            |
 select(maxfd+1, &set, ...) -- modifies set and may modify timeout
          |
          +--> FD_ISSET(listener)? accept
          +--> FD_ISSET(client fd)? read/serve/close
```

- Keep `conn_t *clients[FD_SETSIZE]`, indexed by fd.
- Rebuild the `fd_set` and timeout on every iteration because `select()` may modify them.
- Refuse and close an accepted fd greater than or equal to `FD_SETSIZE`.
- Close and free every remaining client during shutdown.

```bash
make test MODE=select
```

## Task 6: Process per connection — TODO 6 (6%)

In `run_fork()`:

- The child closes the listener, calls `serve_client(fd)`, and calls `_exit(0)`.
- The parent closes its copy of the client fd immediately and continues accepting.
- Do not write `SIGCHLD` code; automatic reaping is already given in `main.c`.

`O_CREAT | O_EXCL` makes the first submission win across processes as well as threads, so no shared-memory mutex is needed.

```bash
make test MODE=fork
```

## Task 7: Linux `epoll` — TODO 7 (10%)

Implement the readiness loop with `epoll_create1`, `epoll_ctl`, and `epoll_wait`:

- Register the listener and nonblocking client sockets.
- Store each `conn_t *` in `event.data.ptr`; use `NULL` to identify the listener.
- Level-triggered `EPOLLIN` is sufficient; edge-triggered mode is not required.
- Keep a registry of open connections so you can clean them all up on shutdown.
- Free each connection exactly once. Closing its fd removes it from the epoll set.

```bash
make test MODE=epoll
```

## How the models differ

| Model | Who waits for a client? | What happens on each iteration? | Main limit in this lab |
|---|---|---|---|
| Iterative | The single server thread | Finish one client before the next `accept()` | One quiet client stalls everyone |
| Thread | One thread per connection | Each thread can block independently | One thread and stack per client |
| Fork | One process per connection | Each child serves one client | Process creation and memory cost |
| `select` | One event-loop thread | Rebuild fd sets and inspect fds up to `FD_SETSIZE` | `FD_SETSIZE`, repeated set construction |
| `poll` | One event-loop thread | Kernel and user code process the poll array | Array work grows with connection count |
| `epoll` | One event-loop thread | Registrations persist; wait returns ready events | More setup and Linux-specific API |

**Readiness is not parallel grading.** `select`, `poll`, and `epoll` only tell you that a socket operation can make progress. `exam_handle_line()` still runs synchronously, and `send_all()` may wait for a slow reader. A 50 ms grading delay therefore stalls the whole event-loop server, while a thread/fork worker stalls only its own connection. Fully nonblocking output and worker offload are optional extensions, not requirements.

## Short notes and full check (5%)

Run:

```bash
make test-all
```

In a short `NOTES.md`, include the iterative 20-student timing and answer:

1. Why does an idle client block the iterative server but not the other models?
2. What is private to each thread/fork worker, and what is shared read-only?
3. Why do duplicate submissions fail without a mutex or shared memory?
4. Why must `select()` rebuild its fd set? What does `poll()` still scan?
5. Why can slow grading stall an `epoll` event loop despite nonblocking sockets?
6. Where did your code handle a partial `recv()` or `send()`?

No large load report is required. An optional 800-client run against `poll` or `epoll` is useful only if you want to explore fd limits (`ulimit -n`).

## Submission and assessment

Submit `protocol/linebuf.c`, `student/student.c`, `exam-server/servers.c`, and `NOTES.md`. Do not submit `bin/`, `data/`, or `results/`.

| Component | Weight |
|---|---:|
| Reliable sending, iterative connection, and student client | 37% |
| Iterative observation | 5% |
| Thread server | 15% |
| `poll` server | 15% |
| `select` server | 7% |
| Fork server | 6% |
| `epoll` server | 10% |
| Short notes | 5% |
| **Total** | **100%** |

## Common mistakes

- Assuming one `recv()` equals one request or one reply.
- Forgetting requests left in the line buffer after processing the first line.
- Passing `&client_fd` from the accept loop to a new thread.
- Closing the client in a fork child but not in the parent.
- Forgetting to make event-loop client sockets nonblocking.
- Removing a `poll` entry without rechecking the entry swapped into its slot.
- Using a client fd greater than or equal to `FD_SETSIZE` with `FD_SET`.
- Treating `epoll` as a worker pool: the handler still runs on the event-loop thread.
- Using `atoi()` or an unchecked `snprintf()` length in new protocol code.
- Modifying the given signal infrastructure instead of using `g_stop` and the supplied helpers.

You may use AI tools for explanation and debugging under the course policy. You must be able to explain every line you submit, especially `connect`, `accept`, `recv`, `send`, `fork`, `close`, and every readiness-loop iteration.
