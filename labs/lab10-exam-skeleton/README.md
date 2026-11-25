# Lab 10 starter: Exam Server

Handout: [`../10lab-exam-server.md`](../10lab-exam-server.md)

```text
protocol/linebuf.c   socket_send_all; framing is given       (TODO 1)
student/student.c    connect + one client exam session       (TODO S1-S2)
exam-server/server.c listener + all six server models        (TODO 2-8)
exam-server/exam.c   request dispatch, data, and grading     (GIVEN)
exam-server/server.h shared server declarations              (GIVEN)
tools/               data generator and test/run scripts     (GIVEN)
```

`server.c` separates student networking into short functions: `create_listening_socket()` contains only `socket`/`bind`/`listen`, while each `run_*_server()` model contains its own `accept()` and dispatch loop. Its `main()` only shows the lifecycle:

```text
init_settings() [given]
        |
create_listening_socket() [you]
        |
run_selected_server() -> one run_*_server() [you]
        |
clean_up() [given]
```

`init_settings()` contains signal and exam/data initialization. `clean_up()` closes the listener and remaining clients and releases the exam database. The `select`, `poll`, and `epoll` models reuse the same given client array, client count, `pollfd` array, and add/find/remove/read API. Thread and process workers own descriptors directly rather than sharing mutable array state. Request dispatch in `exam_process_buffered_requests()`, line extraction, client reply collection, and grading are given so you can focus on socket and concurrency behavior.

## Build and first connection

```bash
make data
make

# Terminal 1: one iterative server
bin/examsrv iterative 5200

# Terminal 2: one student client
PIN=$(awk -F, '$1==1001 {print $3}' data/students.csv)
bin/student 127.0.0.1 5200 1001 "$PIN"
```

The server creates `results/` automatically. Clear old submissions with `make reset`.

## Checks

```bash
make test MODE=iterative
make test MODE=thread
make test MODE=poll
make test-all
```

Do not add signal handlers to the server models. Do not commit `bin/`, `data/`, or `results/`.
