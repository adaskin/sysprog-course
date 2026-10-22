# Swarm Phase 3: Everything Over Sockets, Hardened

**BIL 322 System Programming — Final Project, Phase 3**
**Duration:** 1–2 weeks
**Project:** [Autonomous Drone Swarm Choreographers](../projects/project3-drone-swarm-choreographers.md)
**Submission:** GitHub Classroom (repository) + Google Classroom link, plus a final demo

> **Status: DRAFT.** Details, numbers, and the skeleton code are finalized the
> week this phase is assigned.

---

## 🎯 Context

*Show night, everything wireless.*

Phases 1 and 2 ran on one machine, with pipes and shared memory. Real swarms do not
share memory. In this phase **every link becomes a TCP connection**:

| Link | Phase 1-2 | Phase 3 |
|---|---|---|
| Center ↔ leader | pipes | TCP |
| Performer | thread inside the captain | **separate process**; TCP to its captain (`NEXT`/`GOTO`/`AT`) |
| Center ↔ viewer | shared memory | TCP (`FRAME` and `DRONE` messages) |
| Beat clock | local timer | `BEAT <n>` messages from the center |

Because your protocol code works on file descriptors, this should be a transport change,
not a rewrite. Then comes the hardening: signals, crashes, timeouts, load.

## 📚 Learning Objectives

1. Write a **multithreaded TCP server** and several clients in C.
2. Handle **partial reads and writes** and line framing correctly.
3. Use `poll`/`select` or threads per connection, and justify your choice.
4. Handle `SIGINT`, `SIGTERM`, and `SIGPIPE` correctly (`sigaction`, no unsafe calls in handlers).
5. Detect a dead peer with **heartbeats and timeouts**, and recover (rescue orphaned processes and reassign work).
6. Measure throughput under load and explain the bottleneck.

## 🧰 Prerequisites

- Phases 1 and 2 complete.
- Week 13-14 lectures (sockets, signals).

---

## 🏗️ Architecture for This Phase

```text
 viewer ◀──TCP (FRAME/DRONE)── center (server, port P)
                                  ▲   ▲
                    TCP (HELLO,   │   │  TCP
                    PLAN_REQ,...) │   │
                              leader 0   leader 1 ...
                                  ▲
                    TCP (NEXT/GOTO/AT)
                          follower threads
```

Each process takes its peer addresses from the command line
(`center --port 9000`, `leader --center host:9000 --followers 16`,
`viewer --center host:9000`). Running them on different machines must work.

---

## 🛠️ Task 1: Line-Framed Socket I/O (20%)

Replace the pipe transport with a socket transport behind the same interface:

```c
int  conn_send_line(int fd, const char *line);       /* adds '\n', handles short writes   */
int  conn_recv_line(conn_t *c, char *buf, size_t n); /* buffers partial reads, 0 on EOF   */
```

- Messages longer than 256 bytes are rejected without crashing.
- A test sends the protocol one byte at a time and several messages in one `write`;
  both must parse correctly.
- Use `send(..., MSG_NOSIGNAL)` or ignore `SIGPIPE`, and explain your choice.

## 🌐 Task 2: Server and Clients (30%)

- **Center:** accepts leaders and viewers. Choose and justify the model (thread per
  connection with your Phase 1 queue and planner pool, or a `poll` loop that feeds the
  pool). A full request queue still answers `BUSY`.
- **Leader:** connects to the center, announces itself with `HELLO`, requests a plan for each `THEME`,
  and serves its followers on a local listening socket (the followers connect to it). On each `BEAT <n>` from the center, it tells every follower its target.
- **Performer:** now a separate **process** (the captain `fork`s them, or a launch script starts them) that
  connects to its captain, sends `NEXT <id> <beat>`, receives `GOTO`, moves, and reports `AT`. It has its
  own socket to the center for emergencies, used only for `ORPHANED`.
- **Viewer:** connects to the center and redraws on `FRAME`. The center sends
  frames at most every 100 ms and **drops frames for a slow viewer** instead of
  blocking (a bounded per-viewer send queue). Test with a viewer that sleeps.

## 🚦 Task 3: Signals, Faults, and Orphans (25%)

- `SIGINT` to the center sends `SHUTDOWN` to everyone; all processes exit with status 0
  within 2 seconds, with no zombie processes and no leaked file descriptors (check `lsof`).
- Handlers only set a `volatile sig_atomic_t` flag (or write to a self-pipe); the main
  loops react to it.
- **Fallback routine:** a captain that gets `BUSY` for too long, or loses the center, does not freeze its
  troupe: it switches to a simple built-in local routine (for example, hold formation and pulse the color on
  the beat) and keeps asking for a real plan with randomized exponential backoff. No protocol change.
- **Captain down — the orphan question:** the center receives no `HEARTBEAT` from a captain for 3 seconds (or
  its battery died, or you `SIGKILL`ed it) ⇒ the captain is declared dead. Its performers are now
  **orphans**: their socket to the captain hits EOF or times out, and they send `ORPHANED <id> <old_leader>
  <last_beat>` to the center. The center picks an adopting captain (the least loaded neighbor), sends it
  `ADOPT <follower_id>` for each orphan and a new `ASSIGN`/`THEME` for the dead zone, and answers each
  orphan with `REHOME <id> <host> <port>`. The adopting captain requests a new plan for its enlarged troupe
  and the show continues. Test by killing a captain with `SIGKILL` mid-run; the picture must still form.
  Also handle the edge cases: the orphan hears nothing back (retry with backoff), and the adopting captain
  dies too.
- **Center restart (optional):** leaders retry the connection with randomized exponential
  backoff.

## 📈 Task 4: Load Test and Report (25%)

- `make loadtest` starts one center, 4+ leaders with 50+ followers in total, and a headless viewer,
  performs the provided score, and prints: completion time, number of `BUSY` replies, messages per
  second, the performance score, and the number of safety violations (must be 0).
- Find the bottleneck (planner pool size, queue capacity, locks, or sockets) and change one
  parameter to show its effect.
- `REPORT.md`: protocol changes (if any), concurrency model and why, signal design, fault
  handling timeline, load-test table, AI assistance.

---

## 🎬 Final Demo

Perform a score of your own (a 60-second show to music you choose, within the size limit) on a lab machine, with the
viewer on a second machine if possible, kill one leader live, then shut everything down with `Ctrl-C`.

## 🚀 Bonus (up to +15%)

- B1: Replace the stub planner with a call to a real model through HTTP, behind `plan_section`. The protocol is unchanged.
- B2: New themes for the stub planner (your own formations, for example the crescent and star).
- B3: A web viewer (WebSocket) that shows the swarm in a browser.
- B4: Followers as separate processes (same code, different `main`).

## ✅ Grader's Checklist

```bash
make && make test
make loadtest
# start the swarm, then:
kill -INT <center_pid>; sleep 2; pgrep -f 'center|leader|viewer'   # nothing left
valgrind --leak-check=full --error-exitcode=1 ./center --test
```

Zero-tolerance items: a handler that calls `printf`/`malloc`, zombie processes after shutdown, a
blocked center because of a slow viewer, a missing report.
