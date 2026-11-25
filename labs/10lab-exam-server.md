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
| `bin/examsrv` | One server | Create a listener and serve clients using different models |

The application logic stays the same. Only the server's concurrency model changes.

## How to read the starter

All networking functions are in `exam-server/server.c`, but they have separate, small jobs. Start at the given `main()` and follow the calls:

```text
main() [given]
   |
   +--> init_settings() [given: signals + exam/data initialization]
   |
   +--> create_listening_socket() [TODO 2: socket + bind + listen]
   |
   +--> run_selected_server() [given dispatch]
   |         |
   |         +--> run_iterative_server()                 [TODO 3]
   |         +--> run_thread_per_connection_server()     [TODO 4]
   |         +--> run_poll_server()                      [TODO 5]
   |         +--> run_select_server()                    [TODO 6]
   |         +--> run_process_per_connection_server()    [TODO 7]
   |         +--> run_epoll_server()                     [TODO 8]
   |
   +--> clean_up() [given: close listener/clients + free exam database]
```

You do not need to understand every given line before starting. Implement one small function at a time and use the handout's diagrams to see how it fits into this lifecycle.

`create_listening_socket()` does only `socket()` → `bind()` → `listen()`. It never accepts a client. Each `run_*_server()` contains its own `accept()` logic because that is where the models differ: serve directly, create a thread, create a process, or add the client to a readiness loop.

### Shared client arrays and API

The three single-threaded readiness models use the same given storage in `server.c`:

```text
g_client_array[MAX_CLIENTS]       connection_t records
  +-- input.fd                    accepted client descriptor
  +-- input.in / input.in_len     partial and pipelined input
  +-- session                     login/exam state for this client

g_client_count                    number of active records
g_client_poll_fds[MAX_CLIENTS+1]  listener + poll view of the same records
```

They also use the same given API:

| Function | Job |
|---|---|
| `client_array_add(fd)` | Make an accepted fd nonblocking and store its connection record; takes ownership on success and failure |
| `client_array_find(fd)` | Find a record, used by `epoll` |
| `client_array_remove_at(index)` | Close a client and fill its slot with the last record |
| `client_array_remove_fd(fd)` | Find and remove by descriptor |
| `client_array_prepare_poll(listen_fd)` | Fill the shared `pollfd` array and return its entry count |
| `service_ready_connection(connection)` | Read and process one ready client; return whether it must close |

There is no per-client allocation or separate registry to manage. Only one event-loop thread accesses these arrays, so they need no mutex. The iterative, thread, and process models use the same connection/read operations but do **not** put live workers in the shared array: sharing it between threads or processes would require synchronization and would make the lab more complicated. `clean_up()` closes any readiness clients still in the array.

## Start with one connection

Before writing any concurrent server, make **one student client** talk to **one iterative server**:

```text
 student client                         exam server
      |                                      |
      | socket(); connect()                  | create_listening_socket()
      |                                      | socket(); bind(); listen()
      |------------ TCP connection --------->| accept()
      |                                      | connection_init()
      | LOGIN 1001 1234\n                    |
      |------------------------------------->| connection_read()
      |                                      | exam_process_buffered_requests()
      |                                      | exam_handle_request_line() [given]
      |                 OK Agent0001 0 20\n  |
      |<-------------------------------------|
      | QUESTIONS / SUBMIT / RESULT / QUIT   | same connection
      |------------------------------------->|
      |                                      | close(client_fd)
```

`accept()` returns a **new socket for this client**. The listening socket remains available for later connections. This first working connection is the foundation for every later task.

## What is given, and what is yours?

### Given

- `exam-server/server.c`: `main()`, `init_settings()`, `clean_up()`, shared client arrays/API, connection helpers, the thread worker, model dispatch, and port reporting.
- `exam-server/exam.c`, `exam.h`: request dispatch, CSV loading, login, questions, grading, result storage, and database cleanup.
- `protocol/linebuf.c`: line buffering and extraction.
- `student/student.c`: `read_protocol_reply()` reply collection.
- `tools/`: deterministic data, protocol checks, and concurrent student runner.

`init_settings()` includes all signal infrastructure:

- `SIGINT`/`SIGTERM`: sets `g_server_stop` so server loops can shut down;
- `SIGPIPE`: closed peers do not terminate the whole server;
- `SIGCHLD`: children in the process server are reaped automatically.

Signals have not been covered yet, so they are **given infrastructure, not student work**. Use `socket_send_all()` with `MSG_NOSIGNAL` as described in its comment.

The given parsing and formatting code uses bounded buffers and checked numeric conversions. Invalid ranges, numeric overflow, overlong lines, and formatting truncation are rejected rather than silently accepted.

### Your files

- `protocol/linebuf.c`: TODO 1, `socket_send_all()`.
- `student/student.c`: TODO S1–S2, `connect_to_server()` and `run_exam_session()`.
- `exam-server/server.c`: TODO 2–8, `create_listening_socket()` and six server models.

`server.h` is the only server interface header. Exam-specific work stays in `exam.c`; do not mix grading or CSV parsing into your server loops.

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

For your own request formatting, use bounded `snprintf()` and check its return value whenever the returned length drives `socket_send_all()`. Do not use `atoi()` for numeric reply fields: use the given checked `parse_int_in_range()` helper.

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

## Task 1: Reliable sending, listener, one connection, and one client (37%)

### 1a. Reliable sending — TODO 1

In `protocol/linebuf.c`, implement `socket_send_all()`:

- Loop until every byte is sent.
- Handle partial sends and retry after `EINTR`.
- On nonblocking `EAGAIN`/`EWOULDBLOCK`, wait for `POLLOUT` with `poll()` and continue.
- Use `MSG_NOSIGNAL`.

The framing functions are given. Read `line_buffer_next_line()` and `exam_process_buffered_requests()` to see how they process complete lines without losing a partial line or later messages.

### 1b. Create the listening socket — TODO 2

Implement `create_listening_socket()` in `exam-server/server.c`:

```text
socket(AF_INET, SOCK_STREAM, 0)
             |
             v
setsockopt(SO_REUSEADDR) + initialize sockaddr_in
             |
             v
           bind()
             |
             v
       listen(fd, 1024)
             |
             v
 return listening descriptor to main()
```

- Use `AF_INET`, `htonl(INADDR_ANY)`, and `htons(port)`.
- Port 0 is valid and asks the kernel to choose a free port.
- On every failure, print a useful error, close any open descriptor, and return `-1`.
- Do not call `accept()` here; each server model owns its accept loop.

### 1c. One accepted server connection — TODO 3

In `run_iterative_server()`:

- Call `accept()` and then the given `serve_blocking_client(client_fd)`.
- Only after that client finishes may you accept the next client.
- Retry `accept()` after `EINTR` unless `g_server_stop` is set.
- A client failure must not stop the listener.

The given `serve_blocking_client(fd)` owns the read loop and closes the descriptor. The given `exam_process_buffered_requests()` dispatches requests and sends replies. At this stage, a second client waits until the first client finishes.

### 1d. One student client — TODO S1–S2

In `student/student.c` implement:

1. `connect_to_server()`: `socket`, IPv4 address/port setup, `connect`, and error handling.
2. `run_exam_session()`: `LOGIN`, `QUESTIONS`, think, `SUBMIT`, `RESULT`, and `QUIT`.

`read_protocol_reply()` is given. It collects lines until the reply terminator while preserving bytes belonging to later replies. Answer every question with `A` if you want; parsing individual questions is optional. The final output line must be:

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

## Task 3: Thread per connection — TODO 4 (15%)

In `run_thread_per_connection_server()`:

```text
                         accept loop (main thread)
                                   |
                 +-----------------+-----------------+
                 |                 |                 |
          new heap arg (fd)  new heap arg (fd)  new heap arg (fd)
                 |                 |                 |
          worker thread A    worker thread B    worker thread C
       serve_blocking_client serve_blocking_client serve_blocking_client
          can block          can block          can block
```

- Allocate a separate heap argument for every accepted client.
- Start a detached thread whose argument owns that client fd.
- The worker calls `serve_blocking_client(fd)` and frees its own argument.
- Never pass the address of an `accept`-loop variable to a thread.

One worker may wait for its client while the others continue.

```bash
make test MODE=thread
```

## Task 4: One thread with `poll()` — TODO 5 (15%)

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
       accept new client             connection_read()
                                     + exam_process_buffered_requests()
              |                                 |
              +------------- repeat ------------+
```

- Call `client_array_prepare_poll(listen_fd)` and pass the given `g_client_poll_fds` array and returned count to `poll(..., 1000)`. Index 0 is the listener.
- Service existing clients with `service_ready_connection()` and remove closed clients with `client_array_remove_at()`. The helper performs the read, request dispatch, and close decision.
- Process clients from the last array slot toward the first. This makes the helper's swap-with-last removal safe without rechecking a slot.
- After servicing old clients, accept one client if the listener is readable and call `client_array_add(fd)`. It makes the socket nonblocking and stores its state.
- Retry `EINTR`; ignore nonblocking `EAGAIN`/`EWOULDBLOCK`. Do not allocate or free client records yourself.

```bash
make test MODE=poll
```

# Homework

## Task 5: `select()` — TODO 6 (7%)

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

- Rebuild the `fd_set` and timeout on every iteration because `select()` may modify them: set the listener, then set every `g_client_array[i].input.fd` and track `max_fd`.
- Use the same `service_ready_connection()` and `client_array_remove_at()` API as `poll`, again processing clients in descending index order.
- Accept after servicing old clients. Refuse and close an accepted fd greater than or equal to `FD_SETSIZE`; otherwise call `client_array_add(fd)`.
- Do not create a second client array or free remaining clients yourself; `clean_up()` handles the shared array.

```bash
make test MODE=select
```

## Task 6: Process per connection — TODO 7 (6%)

In `run_process_per_connection_server()`:

- The child closes the listener, calls `serve_blocking_client(fd)`, and calls `_exit(0)`.
- The parent closes its copy of the client fd immediately and continues accepting.
- Do not write `SIGCHLD` code; automatic reaping is given in `init_settings()`.

`O_CREAT | O_EXCL` makes the first submission win across processes as well as threads, so no shared-memory mutex is needed.

```bash
make test MODE=fork
```

## Task 7: Linux `epoll` — TODO 8 (10%)

Implement the readiness loop with `epoll_create1`, `epoll_ctl`, and `epoll_wait`:

- Register the listener with `event.data.fd = listen_fd`; a ready event with that fd identifies the listener.
- After `accept()`, call `client_array_add(client_fd)`, then register that client for `EPOLLIN | EPOLLRDHUP` with `event.data.fd = client_fd`. On registration failure, call `client_array_remove_fd(client_fd)`.
- For a client event, use `client_array_find(fd)` and the same `service_ready_connection()` helper as `select` and `poll`. Remove a closing client with `client_array_remove_fd(fd)`.
- Use fds rather than pointers into `g_client_array`: swap-with-last removal can move a record to another slot.
- Level-triggered `EPOLLIN` is sufficient; edge-triggered mode is not required. Closing a client fd removes it from the epoll set. Close `epoll_fd` on exit; `clean_up()` closes the remaining shared client records.

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

**Readiness is not parallel grading.** `select`, `poll`, and `epoll` only tell you that a socket operation can make progress. `exam_handle_request_line()` still runs synchronously, and `socket_send_all()` may wait for a slow reader. A 50 ms grading delay therefore stalls the whole event-loop server, while a thread/process worker stalls only its own connection. Fully nonblocking output and worker offload are optional extensions, not requirements.

## Short notes and full check (5%)

Run:

```bash
make test-all
```

In a short `NOTES.md`, include the iterative 20-student timing and answer:

1. Why does an idle client block the iterative server but not the other models?
2. What is private to each thread/process worker, and what is shared read-only?
3. Why do duplicate submissions fail without a mutex or shared memory?
4. Why must `select()` rebuild its fd set? What does `poll()` still scan?
5. Why can slow grading stall an `epoll` event loop despite nonblocking sockets?
6. Where did your code handle a partial `recv()` or `send()`?

No large load report is required. An optional 800-client run against `poll` or `epoll` is useful only if you want to explore fd limits (`ulimit -n`).

## Submission and assessment

Submit `protocol/linebuf.c`, `student/student.c`, `exam-server/server.c`, and `NOTES.md`. Do not submit `bin/`, `data/`, or `results/`.

| Component | Weight |
|---|---:|
| Reliable sending, listener, iterative connection, and student client | 37% |
| Iterative observation | 5% |
| Thread server | 15% |
| `poll` server | 15% |
| `select` server | 7% |
| Process server | 6% |
| `epoll` server | 10% |
| Short notes | 5% |
| **Total** | **100%** |

## Common mistakes

- Assuming one `recv()` equals one request or one reply.
- Forgetting requests left in the line buffer after processing the first line.
- Forgetting to close a socket when `bind()` or `listen()` fails.
- Passing `&client_fd` from the accept loop to a new thread.
- Closing the client in a process child but not in the parent.
- Forgetting to make event-loop client sockets nonblocking.
- Removing array entries in ascending order even though removal swaps in the last record; follow the given descending-order rule.
- Using a client fd greater than or equal to `FD_SETSIZE` with `FD_SET`.
- Keeping an `epoll` pointer into `g_client_array` after swap-with-last removal; use `event.data.fd` and `client_array_find()` instead.
- Treating `epoll` as a worker pool: the handler still runs on the event-loop thread.
- Using `atoi()` or an unchecked `snprintf()` length in new protocol code.
- Modifying `init_settings()` or adding signal handlers instead of using `g_server_stop` and the supplied helpers.

You may use AI tools for explanation and debugging under the course policy. You must be able to explain every line you submit, especially `connect`, `accept`, `recv`, `send`, `fork`, `close`, and every readiness-loop iteration. Follow the function call flow and keep each function focused on its documented job.
