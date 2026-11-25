# exam-server

All student networking code is in `server.c`, separated into small functions:

| TODO | Function | Purpose |
|---|---|---|
| 2 | `create_listening_socket()` | Only `socket` → `bind` → `listen`; no `accept` |
| 3 | `run_iterative_server()` | Accept and serve one client at a time |
| 4 | `run_thread_per_connection_server()` | Accept and create a thread |
| 5 | `run_poll_server()` | Accept and run a one-thread `poll()` loop |
| 6 | `run_select_server()` | Accept and run a one-thread `select()` loop |
| 7 | `run_process_per_connection_server()` | Accept and create a process |
| 8 | `run_epoll_server()` | Accept and run a one-thread `epoll` loop |

`main()` is given and only orchestrates the program:

1. `init_settings()` — given signal handling and exam/data initialization.
2. `create_listening_socket()` — student listener.
3. `run_selected_server()` — given dispatch to one student server model.
4. `clean_up()` — given listener, shared-client, and database cleanup.

The listener function never accepts a connection. The selected server model owns its `accept()` behavior; the connection and request-processing operations remain the same.

## Shared readiness-model storage

`select`, `poll`, and `epoll` all use the same given arrays in `server.c`:

- `g_client_array[]`: `connection_t` records containing the client fd, line buffer, and exam session.
- `g_client_count`: number of active records.
- `g_client_poll_fds[]`: the `poll` view, with the listener at index 0.

They also share one API: `client_array_add()`, `client_array_find()`, `client_array_remove_at()`, `client_array_remove_fd()`, `client_array_prepare_poll()`, and `service_ready_connection()`. There is no per-client allocation or model-specific connection registry. `epoll` uses `event.data.fd`, not pointers into an array whose records can move.

Only one event-loop thread accesses these arrays. Thread and process workers own their accepted descriptors directly and use `serve_blocking_client()` instead; putting those workers in a shared mutable array would require locking or inter-process synchronization.

`server.h` is the only server interface header. `exam.c` contains `exam_process_buffered_requests()`, request handling, grading, and result storage.

```bash
bin/examsrv iterative 5200
# Other modes: thread | fork | select | poll | epoll
```
