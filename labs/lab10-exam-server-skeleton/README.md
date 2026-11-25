# Lab 10 starter: Exam Orchestrator

Handout: [`../10lab-exam-server.md`](../10lab-exam-server.md)

```bash
make data          # mock data: 1000 student agents, 3 exams x 20 questions (same for everyone)
make               # build ./examsrv  (the TODO stubs compile, but the servers exit at once)
./examsrv epoll 5200
make test MODE=epoll       # checks + load test for ONE model
make test-all              # all five (unfinished ones are reported as FAIL)
make reset                 # delete results/ (a student can submit only once)
```

| File | Who | What |
|---|---|---|
| `exam.c`, `exam.h` | given | CSV loading, protocol, grading (no networking) |
| `main.c` | given | arguments, listening socket, signals, `conn_read` |
| `conn.c` | **you** | TODO 1-3: framing, `send_all`, `conn_serve` |
| `srv_thread.c`, `srv_fork.c` | **you** | TODO 4-5 |
| `srv_select.c`, `srv_poll.c`, `srv_epoll.c` | **you** | TODO 6-8 |
| `tools/gen_data.py` | given | seeded data generator |
| `tools/loadtest.py` | given | checks + load test (read it: it is a small asyncio client example) |
| `tools/test_mode.sh` | given | starts the server, runs the tester, stops the server |

Do not commit `examsrv`, `data/` or `results/`.
