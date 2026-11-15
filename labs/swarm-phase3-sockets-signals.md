# Swarm Phase 3: Show Night, Everything Over Sockets

**BIL 322 System Programming — Final Project, Phase 3**
**Duration:** after Phase 2, until **Thursday 10 December 2026**. Checkpoint **Thursday 3 December**:
Task 1 (the `tcp:` and `udp:` transports) passes `tests/test_chan net`.
**Project:** [Autonomous Drone Swarm Choreographers](../projects/project3-drone-swarm-choreographers.md)
**Skeleton code:** the same repository as [Phase 2](swarm-phase2-ipc-shared-stage.md),
[`labs/swarm-phase2-3-skeleton/`](swarm-phase2-3-skeleton/): Phase 3 is already in it.
**Submission:** GitHub Classroom (repository) + Google Classroom link, plus the final demo
**Platform:** Linux (a lab machine, a Linux VM, or WSL2). Two machines for the demo if you can.

---

## 🎬 What You Are Building

The same show, with every link on the network. The center becomes a server on one TCP port; captains
connect to it from any machine; every performer is its own process, talking to its captain by radio
(UDP); viewers and operators connect from anywhere. Then you break it on purpose: kill a captain,
freeze another, lose 30% of the radio messages, press `Ctrl-C`. The picture must still form, and
nothing may be left running.

```bash
# machine A: the center, one captain started remotely
bin/center --port 9000 --remote 1 --score data/scores/show.score --followers 46 --tempo 120
# machine B: captain 1 and its 46 performer processes
bin/leader --center A:9000 --id 1
# machine C (or A): your booth, and an operator
bin/viewer --center A:9000 --view gui
bin/swarmctl --center A:9000 kill 1        # captain 1 dies on B: its performers are rescued
```

On one machine, `--port 0` picks a free port and the center starts every captain itself:

```bash
bin/center --port 0 --tempo 240            # prints: [center] listening on tcp port 41234
bin/viewer --center 127.0.0.1:41234        # the terminal view; --view gui for your window
```

---

## 🎯 Context

*Show night, everything wireless.*

Phases 1 and 2 ran on one machine, with pipes, shared memory, a FIFO and a file. Real swarms do not
share memory. In this phase **every link becomes a socket**:

| Link | Phase 2 | Phase 3 |
|---|---|---|
| Center ↔ captain | `chan_fd` over two pipes | `chan_fd` over **one TCP socket**: the same lines; the captain connects back to the center |
| Performer | a thread inside the captain | a **separate process** (`bin/performer`) talking to its captain over **UDP** |
| Center → viewers | stage bus `shm:NAME`, newest frame wins | also **TCP** (`WATCH`): the `FRAME` text message, newest frame wins **per viewer** |
| Operator → center | `fifo:NAME` | also **TCP** (`OPERATE`): the same command lines, each answered `OK` or `ERR` |
| Flight recorder | `file:PATH` | unchanged |
| A dead captain | restarted; its drones start again at home | restarted; its performer **processes survive as orphans** and are adopted by the new captain |
| A frozen captain | the show waits forever | no `HEARTBEAT` for 3 s: declared dead, killed, restarted |

Every link already goes through the channel API (`common/chan.h`), so the captain's code, the
viewer's views and `swarmctl`'s commands do not change: only their addresses do. Your Phase 2
transports keep working next to the new ones: local viewers can still use the stage bus.

**Why UDP for the radio?** A drone's radio link loses packets, and a late position is worthless: by
the time a lost TCP segment is resent, the beat is over and every later message has waited behind it
(head-of-line blocking). So the radio uses datagrams, and the protocol itself copes with loss: every
message carries the beat number, the captain resends `GOTO` until the `AT` arrives, a duplicate is
recognized and ignored. Real drones do the same (MAVLink telemetry runs over UDP), and so do game
engines and QUIC (HTTP/3).

**Modern names for what you build.** One port that serves several kinds of callers, told apart by
their first line, is how an API gateway or an SSH server multiplexes. Heartbeats with a timeout are
a *failure detector*: Kubernetes liveness probes, etcd leases and ZooKeeper sessions work the same
way, and so does their weakness (a slow node looks dead; see Task 4). Rescheduling the orphans of a
dead captain on its replacement is what an orchestrator does with the pods of a dead node, or an
Erlang supervisor with the children of a crashed process. A per-viewer "newest frame wins" buffer is
what a video stream does when the network is slow: drop frames, never fall behind. And the center as
`PR_SET_CHILD_SUBREAPER` plays the role of a container's init process (`tini`), which reaps the
orphans that end up with it.

## 📚 Learning Objectives

1. **Apply** TCP sockets in C (`getaddrinfo`, `socket`, `bind`, `listen`, `accept4`, a `connect`
   with a time limit) behind an existing interface, with partial reads and writes handled.
2. **Apply** UDP sockets for a lossy request/response protocol: datagram boundaries, `recvfrom`
   and `sendto`, connected UDP sockets, ICMP errors, retransmission.
3. **Design** a multithreaded server that serves several kinds of clients on one port, and
   **justify** the concurrency model (a thread per connection or a `poll` loop).
4. **Analyze** how a slow or stopped client can block a server (socket buffers, `SO_SNDTIMEO`) and
   prevent it.
5. **Apply** process control across failures: `fork`/`exec` of performer processes, orphans and
   reparenting, a subreaper, reaping without zombies.
6. **Apply** heartbeats and timeouts to detect a dead or frozen peer, and **evaluate** the false
   alarms they cause.
7. **Apply** signals (`SIGINT`, `SIGTERM`, `SIGPIPE`, `SIGKILL`, `SIGSTOP`) for a clean, complete
   shutdown of a distributed program.
8. **Evaluate** throughput under load and explain the bottleneck with measurements.

## 🧰 Prerequisites

- Phase 2 (due 26 November), in the same repository: Phase 3 builds on it, and `make test` must
  still pass at the end.
- Week 13-14 lectures: sockets (TCP and UDP), signals.

---

## 🏗️ Architecture for This Phase

```text
   machine A                                                       machine B (optional)
 ┌──────────────────────────────── center ──────────────────────┐
 │ clock loop · planner pool · audit · publish                   │
 │ net.c: one TCP port ── HELLO ──▶ a captain's reader thread    │◀── TCP ── leader 1 ── UDP ── performers
 │                     ── WATCH ──▶ a viewer's sender (newest)   │            (--remote 1)    of captain 1
 │                     ── OPERATE ▶ apply_command, OK / ERR      │
 │                     ── ORPHANED▶ handle_orphan: REHOME / LAND │
 │ watchdog: no HEARTBEAT for 3 s ⇒ dead                         │
 │ stage bus (shm), recorder (file), control (fifo): as Phase 2  │
 └──────▲──────────────────────────▲─────────────────▲───────────┘
        │ TCP                      │ TCP             │ TCP
   leader 0 (forked,          viewers, swarmctl   an orphaned performer
   connects back)             (any machine)       (one short connection)
        │ UDP "radio"
   performer 0.0 … 0.15  (processes, children of their captain)
```

The center listens on **one port**. The first line of every connection says who is calling:

| First line | Caller | What follows |
|---|---|---|
| `HELLO <id> 0 <udp_port>` | a captain | the Phase 1 protocol, unchanged; the center reads it in a reader thread as before |
| `WATCH` | a viewer, `swarmctl status` | `FRAME` messages (the text form of a `Frame`), the newest one per viewer |
| `OPERATE` | an operator | command lines; the center answers each with `OK` or `ERR <reason>` |
| `ORPHANED <f> <l> <beat>` | an orphaned performer | one answer, `REHOME <f> <host> <port>` or `LAND <f>`, then the connection closes |
| `QUERY` (bonus B7) | a program with questions | `DRONES`, `DRONE <id>`, `CAPTAIN <l>`; each answered at once with drone coordinates |

### The channel API grows

```c
chan_t *chan_open("tcp:HOST:PORT", CHAN_BOTH);    /* connect (with a time limit): a line channel */
chan_t *chan_listen("tcp::PORT");                 /* socket, bind, listen; port 0 = any free port */
chan_t *chan_accept(chan_t *listener, int ms);    /* one new connection, a line channel; NULL on timeout */
chan_t *chan_open("udp::PORT", CHAN_BOTH);        /* a UDP server: receives from anybody */
chan_t *chan_open("udp:HOST:PORT", CHAN_BOTH);    /* a UDP client: talks to that address */
chan_t *chan_peer(chan_t *udp_server);            /* send-only channel to the last sender */
int     chan_addr(chan_t *c, int which, char *host, size_t n);   /* CHAN_LOCAL / CHAN_REMOTE: port */
void    chan_shutdown(chan_t *c);                 /* wake every thread blocked on c (CHAN_CLOSED) */
```

A connected TCP socket is just a descriptor, so `chan_accept` and the connecting `chan_open` end in
`chan_fd(sock, sock)`: line framing, partial reads, short writes and too-long lines are the code you
have used since Phase 1. A UDP channel is new: **one datagram is one message**, no `\n`, at most
512 bytes, and it may be lost, duplicated or reordered.

### The radio: a captain and its performers

The captain keeps its follower **threads** from Phase 1: a follower thread still waits for the beat,
looks up the plan, releases and claims cells in the grid (the grid lives in the captain). What
changes is the last step: the thread calls `move_performer(idx, b)`, and the captain's radio
(`leader/radio.c`) gets the performer **process** there:

```text
 captain (radio thread + follower threads)              performer f (its own process)
     ◀── NEXT f 12 ───────────────────────────────────  ready for beat 12 (and: "are you there?")
     ─── WAIT f 12 ──────────────────────────────────▶  beat 12 has not started: still alive
  BEAT 12 arrives; follower thread f claims (30, 7)
     ─── GOTO f 12 30 7 9 ───────────────────────────▶  fly to (30, 7), color 9
     ─── GOTO f 12 30 7 9 ─ (resent every ~40 ms) ──▶   (the first one was lost)
     ◀── AT f 12 30 7 ─────────────────────────────────  there; radio_goto returns
  barrier; POS and ACK to the center, as in Phase 1
```

`LAND f` ends a performer: the captain sends it at the end of the show, and the center sends it to
an orphan nobody will adopt. The environment variable `SWARM_UDP_LOSS=30` makes every UDP channel
drop 30% of what it sends, so you can test all of this on one quiet machine.

### When a captain dies

```text
 t = 0      captain 2 is killed (SIGKILL, --battery, or KILL 2). The kernel closes its sockets.
 t ≈ 0      center: its reader gets EOF → your on_leader_exit restarts it (Phase 1 code, unchanged):
            spawn_leader starts "leader --center 127.0.0.1:P --id 2 --adopt" and returns when the
            new captain has said HELLO (with its radio port); then ASSIGN, THEME, BEAT as before.
            The --adopt captain starts NO performers: it waits for the old ones.
 t ≈ 0.25 s performers of captain 2: their next NEXT comes back as ECONNREFUSED (ICMP "port
            unreachable"): orphans. Each one connects to the center: ORPHANED f 2 <last_beat>.
            center (handle_orphan): zone 2 has a live captain → ADOPT f to it, REHOME f host port
            to the performer. The performer reopens its radio to the new captain: NEXT …
 t ≈ 2 s    the new captain starts a fresh performer for any follower nobody came back for.
```

A frozen captain (`kill -STOP`) is harder: no EOF, no ICMP. Its performers hear nothing for 1.5 s
and ask the center, which still believes the captain is alive and sends them back to it (a false
alarm). After 3 s without a `HEARTBEAT` the center's watchdog declares the captain dead, kills it,
and the next round of `ORPHANED` finds the replacement. Retries make the protocol converge.

---

## 🗺️ What Is New in the Source Tree

```text
common/     + chan_tcp.c, chan_udp.c (Task 1), frame_text.c (the FRAME text format, given)
center/     + net.c (Task 2), net.h; center.c: TCP mode, watchdog and handle_orphan (Task 4)
leader/     + radio.c (Task 3), radio.h; leader.c: TCP mode, ADOPT, heartbeat (Task 4)
performer/  + performer.c (Task 3): a drone as a process
viewer/     viewer.c: --center HOST:PORT (given; your views do not change)
ctl/        swarmctl.c: --center HOST:PORT (given)
tests/      test_chan.c: tcp and udp checks; run_tests.sh phase3 (make test3); loadtest.sh
```

| File | What it does | Who |
|---|---|---|
| `common/chan.h`, `chan_impl.h`, `chan.c` | The API above. `chan.c` dispatches `tcp:` and `udp:`, and gives `chan_split_hostport` (`"[::1]:9000"` too) and `chan_sockaddr` to the transports. | given |
| `common/chan_tcp.c` | `tcp:`: connect with a time limit, the listener, accept. | **Task 1** |
| `common/chan_udp.c` | `udp:`: client, server, `chan_peer`, `SWARM_UDP_LOSS`. | **Task 1** |
| `common/frame_text.*` | `frame_send_text`, `frame_recv_text`: a `Frame` as one text message. | given |
| `common/protocol.*` | The Phase 3 messages are already parsed and formatted (`NEXT` … `OPERATE`). | given |
| `center/net.h` | The interface between the center and its server, both ways. | given |
| `center/net.c` | `net_start`, `net_publish`, `net_stop`: accept, dispatch by the first line, viewers, operators, orphans. | **Task 2** |
| `center/center.c` | New, given: `--port`, `--remote`, `--heartbeat-ms`; `spawn_leader` in TCP mode (returns when the captain has connected); `center_attach_leader`, `center_command`, `center_orphan`; a subreaper; `last_heard`. | given + **Task 4**: `watchdog_main`, `handle_orphan` |
| `leader/radio.h` | The radio's interface (and the radio protocol). | given |
| `leader/radio.c` | The captain's side of the radio: start performers, answer `NEXT`, resend `GOTO`, collect `AT`, `ADOPT`, `LAND`. | **Task 3** |
| `leader/leader.c` | New, given: `--center`, `--adopt`, the TCP connection and `HELLO`, `ADOPT`, `move_performer`. | given + **Task 3**: one call in your `follower_main`; **Task 4**: `heartbeat_main` |
| `performer/performer.c` | One drone as a process: the radio, orphan rescue. | **Task 3** |
| `viewer/viewer.c`, `ctl/swarmctl.c` | `--center HOST:PORT`: `WATCH` and `OPERATE` over TCP. | given |
| `tests/test_chan.c` | `tcp`, `udp` (31 checks; `net` runs both). | given |
| `tests/run_tests.sh` | `phase3`: `sockets net radio faults vg3`. | given; add yours (Task 5) |
| `tests/loadtest.sh` | `make loadtest`: one table row per run. | given |

### Processes and threads

```text
 center (process; a subreaper)                captain i (process)                  performer i.f (process)
   main:      clock loop, audit, publish        main:     reads the center (TCP)     one thread: the radio
   reader i:  captain i's lines (TCP now)       thinker, follower threads (Phase 1) (NEXT, GOTO, AT,
   workers:   the planner pool                  radio:    answers NEXT, collects AT    ORPHANED, REHOME)
   net:       accept + one thread per           heartbeat: HEARTBEAT every 500 ms
              viewer, operator, orphan
   watchdog:  silent captains                 viewer, swarmctl (processes, any machine)
   control, signals: as in Phase 2
```

### Where your tasks plug in

| Task | You write | It is called from | Test it alone with |
|---|---|---|---|
| 1 | `common/chan_tcp.c`, `common/chan_udp.c` | everything below | `tests/test_chan tcp`, `tests/test_chan udp` |
| 2 | `center/net.c` | `main` and `publish` (center) | `tests/run_tests.sh net` (works without Task 3: the followers stay threads) |
| 3 | `leader/radio.c`, `performer/performer.c`, one call in `follower_main` | the captain's `main`, `move_performer` | `tests/run_tests.sh radio` |
| 4 | `heartbeat_main` (leader), `watchdog_main`, `handle_orphan` (center) | the given TCP code | `tests/run_tests.sh faults` |
| 5 | tests, measurements, `REPORT.md` | `make test3`, `make loadtest` | |

---

## 🚀 Getting Started

**1. Finish Phase 2 first** (26 November). Phase 3 is in the same skeleton; `make` already builds
`bin/performer` and the new files. Until Task 1 is done, `--port` says
`cannot serve tcp::0 (Phase 3, Tasks 1 and 2 not done?)` and everything without `--port` works as in
Phase 2.

**2. Suggested order.** Task 1 → (checkpoint, 3 December) → Task 2 → Task 3 → Task 4 → Task 5.
Tasks 2 and 3 are independent once Task 1 works: a team of two can split them. Without a radio
(`radio_open` returns -1) the captains say `no radio ... followers stay threads` and the show runs
over TCP with Phase 2 followers, so Task 2 can be finished and tested before Task 3.

**Useful commands.**

```bash
make test3                                         # all Phase 3 checks (make test: Phase 2)
tests/test_chan tcp                                 # one transport alone: tcp | udp | net
tests/run_tests.sh net                              # one part: sockets | net | radio | faults | vg3
bin/center --port 0 --tempo 240                     # prints the port it got
bin/center --port 9000 --paused                     # waits for: bin/swarmctl --center 127.0.0.1:9000 resume
bin/viewer --center 127.0.0.1:9000                  # the terminal view (--view gui: your window)
bin/swarmctl --center 127.0.0.1:9000 status         # also: watch, pause, resume, tempo N, kill ID, stop
nc 127.0.0.1 9000                                   # then type WATCH (or OPERATE, then PAUSE)
SWARM_UDP_LOSS=30 bin/center --port 0 --tempo 0     # a noisy radio
bin/center --port 0 --battery 1:20                  # captain 1 dies at beat 20: watch the orphans
pstree -p $(pgrep -n center)                        # the processes, and who is whose parent
ss -tunap | grep -E 'center|leader|performer'       # every socket of the show
make loadtest                                       # the load test (Task 5)
```

---

## 🛠️ Task 1: The TCP and UDP Transports (20%, checkpoint 3 December)

Write `common/chan_tcp.c` and `common/chan_udp.c`. Their headers list the rules; `tests/test_chan
net` checks every one of them (31 checks).

**TCP.** `tcp:HOST:PORT` connects; `chan_listen("tcp::PORT")` listens; the listener's `accept`
returns `chan_fd(sock, sock)`.

- `getaddrinfo` for every address (a host name or an IP, IPv4 or IPv6); try each result in turn.
- A connect that cannot hang: non-blocking `connect`, `poll` for `POLLOUT`, then `SO_ERROR`. A port
  where nobody listens fails at once with `errno == ECONNREFUSED` (callers look at it).
- `SOCK_CLOEXEC` on every socket (the center and the captains fork), `SO_REUSEADDR` on the listener.
- A peer that never reads must not block a sender forever: `SO_SNDTIMEO`. Consider `TCP_NODELAY`.
- The listener is a channel with its own operations (`accept`, `shutdown`, `addr`, `close`).
- `SIGPIPE`: the given mains ignore it. Could you use `MSG_NOSIGNAL` instead? What would change?

**UDP.** `udp::PORT` is a server (bound, receives from anybody, answers the last sender);
`udp:HOST:PORT` is a client (a *connected* UDP socket); `chan_peer(server)` is a send-only channel
to the last sender that shares the server's socket.

- One datagram = one message, at most 512 bytes. A datagram bigger than the buffer: `CHAN_TOOBIG`
  (`recvfrom` with `MSG_TRUNC` tells the real length).
- A connected UDP socket reports an ICMP "port unreachable" as `ECONNREFUSED` on a later call:
  return `CHAN_CLOSED`. It is how a performer notices a dead captain at once.
- `SWARM_UDP_LOSS=PCT`: drop that share of the sends (`rand_r`, a seed per channel).

**In the report:** why `chan_fd` was enough for TCP but not for UDP; what happens to a 3000-byte
`chan_send` when the peer does not read (measure it: the test prints it); `MSG_NOSIGNAL` or
`SIG_IGN`.

## 🌐 Task 2: The Center as a TCP Server (25%)

Write `center/net.c` (`center/net.h` is the contract). The given center calls `net_start("tcp::P")`
before it starts the captains, `net_publish(&frame)` after every beat, and `net_stop()` at the end.

- **Dispatch.** Read the first line of every connection (at most 2 s: a caller that says nothing must
  not hold anything up) and hand it over: `HELLO` → `center_attach_leader` (the center owns the
  channel from then on; refused: close it), `WATCH` → a viewer, `OPERATE` → an operator,
  `ORPHANED` → `center_orphan`. Anything else: close.
- **Viewers: the Phase 2 bus rules, per connection.** Each viewer has a slot that holds only the
  newest frame, and its own sender. `net_publish` overwrites the slots and wakes the senders; it
  never touches a socket, so it cannot block, whatever a viewer does. A slow viewer skips frames; a
  stopped one (`SIGSTOP`) blocks only its own sender, until `SO_SNDTIMEO` drops it. A viewer that
  connects late gets the newest frame at once. (The kernel's socket buffers are a queue too: the
  given `viewer.c` drains them and draws only the newest frame.)
- **Operators.** Every line goes to `center_command`; answer `OK` or `ERR <reason>`.
- **Concurrency model: your choice, justified.** A thread per connection (simple with blocking
  channels) or one `poll` loop (scales to thousands of clients; needs non-blocking sends and buffers
  per viewer). Bonus B6 is the other one, measured.
- **`net_stop`.** Stop accepting, give viewers up to a second to send what was published (the final
  frame), `chan_shutdown` the rest, join every thread, close and free everything.

**In the report:** your model and why; what a stopped viewer costs the center (a thread? memory?
for how long?); a sequence diagram of one beat with two viewers, one of them slow.

## 📡 Task 3: Performers on the Radio (25%)

Write `leader/radio.c` (the interface and the protocol are in `leader/radio.h`), `performer/
performer.c`, and add one call to your `follower_main`: `move_performer(idx, b)` after the claim,
before the second barrier.

- **The captain** (`radio.c`): `radio_open` opens `udp::0` and starts a radio thread;
  `radio_launch` starts one `bin/performer` per follower (`fork` + `execv`; build the arguments
  before `fork`); the radio thread answers `NEXT` with the current `GOTO` or with `WAIT`, and
  collects `AT`; `radio_goto` sends `GOTO` and resends it until the `AT` for that beat arrives (a
  condition variable with a ~40 ms timeout, not a sleep); `radio_close` sends `LAND` (three times),
  then reaps its children (`waitpid`, then `SIGKILL` after a second).
- **Who is who.** A slot remembers its performer's address (`chan_addr(radio, CHAN_REMOTE, …)` after
  each `chan_recv`). Datagrams from anybody else for that slot get `LAND`, unless the slot is *open*:
  after `ADOPT`, or when its performer stopped answering. Then the next `NEXT` takes the slot.
- **The performer** (`performer.c`): `NEXT`, then `GOTO` → `AT`, `WAIT` → nothing, `LAND` → exit;
  nothing for 250 ms → `NEXT` again; nothing at all for 1.5 s, or `CHAN_CLOSED` → it is an orphan
  (Task 4 wires up the center's side; the performer's side is here): connect to the center,
  `ORPHANED`, then `REHOME` (reopen the radio) or `LAND` (exit); no answer → retry with randomized
  exponential backoff, then give up and exit. A center that refuses the connection is gone: exit.
- `SWARM_UDP_LOSS=30` must not break the show: only slow it down.

**In the report:** the radio protocol as a state machine (the performer's and the captain's side);
why every message carries the beat; what `SWARM_UDP_LOSS=30` costs (measure it); why the captain
keeps its follower threads instead of letting the performers decide.

## 🚑 Task 4: Heartbeats, Orphans and a Clean Stop (15%)

Three functions in the given TCP code; the hard part is the timing, not the amount of code.

- **`heartbeat_main`** (leader): `HEARTBEAT <id>` every 500 ms until the show stops (a timed wait
  on `beat_cv`, not a sleep: the captain must exit at once at the end).
- **`watchdog_main`** (center): every ~100 ms, a captain whose `last_heard` is older than
  `--heartbeat-ms` (3000) is declared dead: log it, `SIGKILL` it if it is ours, and `chan_shutdown`
  its link, so its reader gets `CHAN_CLOSED` and your Phase 1 `on_leader_exit` restarts it.
- **`handle_orphan`** (center): wait (`wait_for(captain_settled, …)`, at most 5 s) until the zone
  has a live captain; then `ADOPT f` to it **first**, and `REHOME f host port` to the orphan (the
  captain's radio; if that captain runs on this machine, the address the orphan used to reach the
  center). No captain (`--no-restart`, shutting down, timeout): `LAND f`.
- **The edge cases** (`make test3` tries them): the orphan hears nothing back (it retries); the
  adopting captain dies too (the orphans move again); a frozen captain (`SIGSTOP`); 30% loss and a
  crash together; `Ctrl-C` in the middle of a rescue.
- **`Ctrl-C`** on the center ends everything within 2 seconds, on every machine: captains get
  `SHUTDOWN`, performers get `LAND` (or find out by themselves), viewers see the final frame and
  `CHAN_CLOSED`. No zombies, no leftover processes (`pgrep performer`), no descriptors left open
  (`valgrind --track-fds=yes`, or `ls -l /proc/<pid>/fd` before the end). The given center is a
  **subreaper** (`prctl(PR_SET_CHILD_SUBREAPER)`): orphaned performers become its children, and it
  reaps them at the end.

**In the report:** a timeline (as above, with your measured times) of a `SIGKILL` and of a
`SIGSTOP` of a captain; how you chose the 500 ms / 1.5 s / 3 s timeouts and what a shorter or longer
one would cause; can two performers ever answer for the same follower, and what then?

## 📈 Task 5: Load Test, Two Machines, Tests and Report (15%)

- **`make loadtest`** (`tests/loadtest.sh`) runs the long score at tempo 0 with 4 captains × 16
  performers and a TCP viewer, and prints one row: drones, beats, seconds, beats/s, `BUSY` replies,
  late beats, messages from the captains and messages/s, frames the viewer drew, violations (must be
  0), mosaic accuracy. Find the **bottleneck**: change **one** parameter at a time (`--workers`,
  `--queue-cap`, `--think-ms`, `LEADERS`/`FOLLOWERS`, `SWARM_UDP_LOSS`, your own `RETRY_MS`) and
  put the rows in a table. Explain the shape with evidence (`top -H`, `perf top`, `strace -c`,
  timestamps in your logs), not guesses.
- **Two machines.** Run the center on one machine and at least one captain (`--remote L`) on
  another; watch from a third if you can. Kill the remote captain: the center starts a local
  replacement (failover) and the remote performers are rehomed across the network. Lab machines may
  filter ports: ask, or use the port you are given. Put the commands and a screenshot in the report.
- **`make test3`** passes, `make test` (Phase 2) still passes, and you add **two tests of your own**
  to `tests/run_tests.sh` (for example: a viewer that connects in the middle of the show, an
  operator who sends 1000 commands, a performer killed by hand, 30% loss plus a captain kill).
- **`REPORT.md`**: the answers asked in Tasks 1-4, the load-test table and your explanation, one
  race or deadlock you hit and how you found it, what you would do for 10,000 drones, and the AI
  assistance you used (what you asked, what you kept, what you had to fix).

---

## 🎬 Final Demo (in the lab, about 8 minutes)

Perform the heart show (or a score of your own) on a lab machine, with a captain or the viewer on a
second machine if possible. Live: show the processes (`pstree`), kill a captain (`swarmctl kill`, or
`kill -9` on its machine), freeze another one (`kill -STOP`), and let the picture form anyway. Then
`Ctrl-C`, and show that nothing is left (`pgrep`, `ss`). Be ready to explain any line of your code.

## 🧯 Hints

- `ss -tlnp` shows who listens on which port; `ss -tnp` the TCP connections; `ss -unap` the UDP
  sockets. A test that crashed may leave a center listening: `pkill -f 'center --port'`.
- `strace -f -e trace=network bin/center --port 0 --tempo 0` shows every socket call;
  `-e trace=process` every `fork`, `execve`, `wait4` and `kill`.
- `tcpdump -i lo -A udp` (or Wireshark) shows the radio's datagrams; with `SWARM_UDP_LOSS` you
  see the resends.
- "Address already in use" after a restart: `SO_REUSEADDR` before `bind`.
- A connected UDP socket reports a dead peer *on the next call*, `send` or `recv`, whichever comes
  first. Check both.
- `chan_recv` on a UDP server and `chan_addr(…, CHAN_REMOTE, …)` must be in the same thread, with
  nothing in between: the "last sender" changes with every datagram.
- After `fork` in a multithreaded process only async-signal-safe functions are allowed until `exec`:
  no `printf`, no `malloc`, no mutex.
- A zombie in `ps` (`Z`, `<defunct>`) is a child nobody waited for. An orphan's parent is the
  subreaper (the center), or `systemd --user`/`init` without one: `ps -o pid,ppid,stat,cmd`.
- Timeouts make tests slow and flaky: wait on conditions (a condition variable, `poll`) with a
  deadline, never `sleep` and check.

## 🚀 Bonus (up to +15%)

- B1: **Fallback routine.** A captain that gets `BUSY` for too long, or loses the center, does not
  freeze its troupe: it switches to a built-in local routine (hold the formation and pulse the
  color on the beat, driven by its own clock) and keeps asking for a real plan with randomized
  exponential backoff. No protocol change.
- B2: **Center restart.** The captains survive a center crash: they reconnect with backoff, say
  `HELLO` again, and the restarted center adopts them (it needs to learn the state from them).
- B3: **A booth per captain, on its machine.** `bin/viewer --center A:9000 --captain 2` on captain 2's
  machine: its zone, its performers, its own controls (and Qt 6 or GTK 4, if you did not do Phase 2's
  B2).
- B4: **A web viewer.** The center (or a small gateway program) speaks WebSocket, and a browser shows
  the show.
- B5: **A real model behind `plan_section`** (an LLM or a planner service over HTTP), with
  timeouts and `BUSY` when it is slow. The protocol does not change.
- B6: **The other server model** (Task 2): `poll` or `epoll` instead of threads (or the reverse),
  with a measured comparison at 100 viewers.
- B7: **A view API.** `WATCH` pushes every frame to a viewer. Add the other style, request and
  response: a program asks the center a question and gets the coordinates of the drones it asked
  for, now. A dashboard, a script or a scoring service needs this, not a stream. On the same port,
  a new first line:

  ```text
  QUERY                     the first line: questions follow, one per line; the connection stays open
  DRONES                →   DRONES <beat> <n>, then n lines  P <id> <leader> <x> <y> <color> <state>
  DRONE <id>            →   P <id> <leader> <x> <y> <color> <state>   (or ERR no such drone)
  CAPTAIN <leader_id>   →   DRONES <beat> <k>, then the k drones of that captain
  anything else         →   ERR bad request
  ```

  `<state>` is a word (`ok`, `lost`, `orphan`). The answers come from the newest frame the center
  published (`net_publish` already gets every one): copy it under a short lock and answer from the
  copy, so a question never takes the center's own locks or waits for a beat, and a client that
  asks and never reads is dropped like a slow viewer. Give `swarmctl` a client
  (`swarmctl --center HOST:PORT drones [ID | --captain L]`); `nc` works too:
  `printf 'QUERY\nDRONE 17\n' | nc -q1 127.0.0.1 9000`. Show in your tests that 1000 questions
  during a tempo-0 show do not slow it down, and that every answer matches the recorded frame of its
  beat. Going further: answer the same questions over HTTP with JSON (`GET /drones`,
  `GET /drones/17`, `GET /captains/2/drones`), so `curl` and a browser (B4) can use it.

## ✅ Grader's Checklist

```bash
make && make test && make test3          # ALL CHECKS PASSED, twice
make loadtest                            # 0 violations
bin/center --port 9000 --tempo 120 &  bin/viewer --center 127.0.0.1:9000 --view gui
bin/swarmctl --center 127.0.0.1:9000 kill 2; kill -STOP $(pgrep -f 'leader --id 1 ')
kill -INT %1; sleep 2; pgrep -fa 'center|leader|performer|viewer'      # nothing left
valgrind --leak-check=full --track-fds=yes bin/center --port 0 --tempo 0 --battery 1:20
```

Zero-tolerance items: a center that a viewer, an operator or a silent caller can block; a performer
or captain left running after the show or after `Ctrl-C`; zombie processes; a signal handler that
calls `printf` or `malloc`; busy-waiting or `sleep`-and-check loops; a missing report.
