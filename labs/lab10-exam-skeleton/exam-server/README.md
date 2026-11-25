# exam-server: the server program

One server, many students. `exam.c` (given) holds the exam protocol and grading; you write how the server waits for and serves clients:

| TODO | File | |
|---|---|---|
| 3 | `conn.c` | `conn_serve()`: process the request lines in the buffer |
| 4 | `srv_iterative.c` | one client at a time |
| 5 | `srv_thread.c` | thread per connection |
| 6 | `srv_poll.c` | one thread, `poll()` |
| 7 | `srv_epoll.c` | one thread, `epoll` |
| 8 | `srv_fork.c` | process per connection (homework) |
| 9 | `srv_select.c` | one thread, `select()` (homework) |

Framing and `send_all` (TODO 1–2) are in `../common/linebuf.c`.

```bash
bin/examsrv thread 5200      # MODE = iterative | thread | fork | select | poll | epoll
```
