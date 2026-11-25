# exam-server

Protocol dispatch in `conn_serve()` is **given**. All student server code is in `servers.c`:

| TODO | Function | Model |
|---|---|---|
| 2 | `run_iterative()` | One accepted client at a time |
| 3 | `run_thread()` | Thread per connection |
| 4 | `run_poll()` | One-thread `poll()` loop |
| 5 | `run_select()` | One-thread `select()` loop |
| 6 | `run_fork()` | Process per connection |
| 7 | `run_epoll()` | One-thread `epoll` loop |

`server.h` declares the shared connection state, helpers, and these functions. `main.c` gives the listening socket, `serve_client()`, `set_nonblocking()`, and all signal handling. `exam.c` gives the application protocol and grading.

```bash
bin/examsrv iterative 5200
# Other modes: thread | fork | select | poll | epoll
```
