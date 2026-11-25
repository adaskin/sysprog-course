# Lab 10 starter: Exam Server

Handout: [`../10lab-exam-server.md`](../10lab-exam-server.md)

```text
protocol/linebuf.c    send_all; framing helpers are given       (TODO 1)
student/student.c     connect + one client exam session         (TODO S1-S2)
exam-server/servers.c all six server models                     (TODO 2-7)
exam-server/main.c    listener, helpers, signals                (GIVEN)
exam-server/exam.c    exam protocol, data, grading              (GIVEN)
tools/                data generator and test/run scripts       (GIVEN)
```

`server.h` is the only server interface header. The six server models are separate sections of `servers.c`, not separate files. `conn_serve()`, line extraction, client reply collection, and application parsing are given so you can focus on socket and concurrency behavior.

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

Signals are given infrastructure because they are covered later in the course. Do not add signal handlers to the server models.

Do not commit `bin/`, `data/`, or `results/`.
