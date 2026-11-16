# Swarm Phase 3: Show Night, Every Drone Its Own Machine

**BIL 322 System Programming — Final Project, Phase 3**
**Duration:** until **Thursday 10 December 2026** (assigned together with [Phase 2](swarm-phase2-ipc-shared-stage.md), due 26 November, in the same repository).
Checkpoint **Thursday 3 December**: `make test3` (`bin/test_chan tcp udp`) passes.
**Project:** [Autonomous Drone Swarm Choreographers](../projects/project3-drone-swarm-choreographers.md)
**Skeleton:** the same as Phase 2, [`labs/swarm-phase2-3-skeleton/`](swarm-phase2-3-skeleton/): read its [`README.md`](swarm-phase2-3-skeleton/README.md) and [`PROTOCOL.md`](swarm-phase2-3-skeleton/PROTOCOL.md) (Sections 1.1, 1.2, 1.5, 3.3, 3.4 are new for you)
**Submission:** GitHub Classroom (repository) + Google Classroom link, plus the final demo
**Platform:** Linux (a lab machine, a Linux VM, or WSL2). Two machines for the demo if you can.

---

## 🎬 What You Are Building

The same show, on a network. The center, every captain, every drone and every viewer is a
program of its own, on a machine of its own, and they find each other through one address: the
center's `tcp:HOST:PORT`. Every drone becomes a **performer** process that flies by radio (UDP),
asking its captain for its next move. Then you break it on purpose: kill a captain, freeze
another, lose 20% of the radio's datagrams, kill a drone, press `Ctrl-C`. The picture must still
form, no two drones may ever touch, and nothing may be left running.

On real machines:

```bash
# machine A (10.0.0.1): the center
bin/center --listen tcp::9000 --score data/scores/show.score --captains 4 --followers 46 --record file:/tmp/show.rec
# machines B, C, D, E: one captain each, and 46 performers next to it (or anywhere)
bin/captain --center tcp:10.0.0.1:9000 --radio udp::0
for i in $(seq 46); do bin/performer --center tcp:10.0.0.1:9000 & done
# any machine: watch, steer
bin/viewer --from tcp:10.0.0.1:9000 --gui
bin/swarmctl --center tcp:10.0.0.1:9000 kill 2
```

On one machine, `SWARM_BIND` gives each program an address of its own (all of `127.0.0.0/8` is
yours on Linux), so `ss`, `tcpdump` and your logs show the same picture as on many machines:

```bash
SWARM_BIND=127.0.0.2  bin/center --listen tcp::9000 --score data/scores/short.score --captains 2 --followers 4
SWARM_BIND=127.0.0.11 bin/captain --center tcp:127.0.0.2:9000 --radio udp::0
SWARM_BIND=127.0.0.12 bin/captain --center tcp:127.0.0.2:9000 --radio udp::0
for i in $(seq 8); do SWARM_BIND=127.0.1.$i bin/performer --center tcp:127.0.0.2:9000 & done
SWARM_BIND=127.0.0.50 bin/viewer --from tcp:127.0.0.2:9000 --text
```

---

## 🎯 Context

*Show night, everything wireless.*

In Phase 2 the programs already met by address, but on one machine, and a captain's drones lived
inside it. Now every link crosses a network, and every drone is a machine that can be switched on
late, crash, or lose its radio:

| Link | Phase 2 | Phase 3 |
|---|---|---|
| captain, viewer, operator → center | `local:PATH` | `tcp:HOST:PORT`: the same lines, from any machine |
| a drone | a thread (or whatever you chose) inside its captain | a **performer** process: `JOIN`s the show through the center, then flies by radio |
| captain ↔ its drones | memory | **`udp:`**: `NEXT`, `GOTO`, `WAIT`, `LAND` (PROTOCOL.md 3.3) |
| center → viewers | `shm:` on the machine, `local:` text frames | also `tcp:` text frames, to any machine |
| a dead captain | its drones hover; a standby takes the zone | the same, and its performers, now **orphans**, ask the center where to go and are **rehomed** to the new captain |
| a dead drone | (it cannot die alone) | its captain reports it `LOST`; a **spare** performer takes its slot |

Your channel API pays off now: `chan_open("tcp:...")` instead of `chan_open("local:...")`, and the
center, captain and viewer of Phase 2 work across machines. Phase 2's schemes keep working next to
the new ones: a viewer on the center's machine can still read the stage bus.

**Why UDP for the radio?** A drone's radio loses packets, and a late position is worthless: by the
time TCP has resent a lost segment, the beat is over and every later message has waited behind it
(head-of-line blocking). So the radio uses datagrams, and the protocol copes with loss itself: the
performer always asks (`NEXT f b`) and keeps asking every 50 ms until it gets an answer; every
message carries its beat, so a duplicate or a late answer is recognized and ignored; and the
captain only ever answers, so it keeps no timers and works through NAT. Real drones do the same
(MAVLink telemetry over UDP), and so do game engines and QUIC (HTTP/3).

**Modern names for what you build.** One port that serves several kinds of callers, told apart by
their first line, is how an API gateway or an SSH server multiplexes. Heartbeats with a deadline
are a *failure detector*: Kubernetes liveness probes, etcd leases and ZooKeeper sessions work the
same way, and share its weakness (a slow node looks dead; see Task 4). A performer that asks the
center where to go after its captain died is *client-side service discovery*; a spare that waits
for a slot is a *warm standby*; the center rehoming orphans is an orchestrator rescheduling the
pods of a dead node. Pull-based delivery (the performer asks, the captain answers) is how
Prometheus scrapes and how Kafka consumers read: the side that may be slow sets the pace.

## 📚 Learning Objectives

1. **Apply** TCP sockets in C (`getaddrinfo`, `socket`, `bind`, `listen`, `accept`, `connect` with a
   time limit) behind your own interface, with short reads and writes handled.
2. **Apply** UDP sockets for a lossy request/response protocol: datagram boundaries, `recvfrom` and
   `sendto`, connected UDP sockets and their ICMP errors, retransmission, duplicates.
3. **Design** a server that serves several kinds of clients on one port, and **justify** its
   concurrency model (a thread per connection, or a `poll` loop).
4. **Analyze** how a slow, stopped or silent client can block a server (socket buffers, timeouts)
   and prevent it.
5. **Apply** heartbeats and timeouts to detect a dead or frozen peer, and **evaluate** the false
   alarms they cause.
6. **Apply** signals (`SIGINT`, `SIGTERM`, `SIGPIPE`; and `SIGKILL`, `SIGSTOP` as failures) for a
   clean, complete stop of a program spread over many machines.
7. **Evaluate** throughput under load and explain the bottleneck with measurements.

## 🧰 Prerequisites

- Phase 2 in the same repository: `make test2` and `make check2` must still pass at the end.
- Week 13-14 lectures: TCP and UDP sockets, signals.

---

## 🏗️ Architecture for This Phase

```text
  machine A                     machine B (127.0.0.11)                 machines P1..Pn (127.0.1.x)
 ┌────────────────────┐  tcp   ┌──────────────────────┐  udp (radio)  ┌──────────────┐
 │ bin/center         │◄──────►│ bin/captain          │◄─────────────►│ bin/performer│ × n
 │ tcp::9000          │  lines │ --radio udp::0       │  NEXT / GOTO  │ one drone    │
 │ zones · planner ·  │        │ HELLO <radio port>   │  WAIT / LAND  └──────┬───────┘
 │ beat clock · audit │        └──────────────────────┘                      │ tcp, briefly:
 │ frames · rescue    │◄─────────────────────────────────────────────────────┘ JOIN, ORPHANED
 └─┬───────┬──────────┘
   │ tcp   │ tcp (WATCH: text frames)          file: (recording)   shm: (bus, machine A only)
   ▼       ▼
 swarmctl  bin/viewer (any machine)
```

### A performer's life

```text
JOIN ──► REHOME <zone> <f> <host> <port>  ──► NEXT f 0 ──► GOTO / WAIT ──► NEXT f b+1 ──► ... ──► LAND: exit
  │      (or wait as a spare, or LAND -1)                         │
  │                                     no answer for ORPHAN_MS (1.5 s), or the captain's socket is gone
  │                                                               ▼
  └──────────────────────────────────── ORPHANED <f> <zone> <last_beat> ──► REHOME (a new captain) or LAND
```

It talks to the center only twice in its life, on short connections; everything else is radio.
The captain answers each `NEXT` with its newest `GOTO` for that slot, or `WAIT`; it never sends
first. A `GOTO` is confirmed when the same performer asks for the next beat.

### When a captain dies

1. The center sees its connection close, or a second of silence: the captain is **dead**
   (`dead=` in the `RESULT` line). Its drones hover in the frames as `DRONE_LOST`.
2. If a **standby** captain is connected, the center gives it the zone at once: `ASSIGN` with the
   same zone number, `PLACE` lines with the drones' last cells, the sections (`takeovers=`).
3. The performers hear nothing for 1.5 s: they are **orphans**. Each one asks the center,
   `ORPHANED f zone last_beat`. The center sends the new captain `ADOPT f` and answers the
   performer `REHOME` with the new captain's radio address (`rehomed=`). The performer says
   `NEXT f ...` there, and the new captain accepts it because of the `ADOPT`.
4. With no standby, the zone stays frozen, and the orphans get `LAND` (`landed=`).

`CAPTAIN_DEAD_MS` (1 s) is shorter than `ORPHAN_MS` (1.5 s) on purpose: by the time an orphan
asks, the center already knows.

---

## 🚀 Getting Started

```bash
cat PROTOCOL.md                            # again: 1.1, 1.2, 1.5, 3.3, 3.4, 5 are what is new
make test3                                 # fails until Task 1 is done
tests/check.sh list                        # the five Phase 3 scenarios
```

A plan to the end (each step ends with something you can run):

| By | Have this working |
|---|---|
| Sat 28 Nov | `tcp:` passes `bin/test_chan tcp`; your Phase 2 show runs with `--listen tcp::9000` and nothing else changed |
| Tue 1 Dec | `udp:` passes `bin/test_chan udp`, with `SWARM_BIND` and `SWARM_DROP` |
| **Thu 3 Dec** | **checkpoint**: `make test3`; one performer `JOIN`s and flies for one radio captain |
| Sun 6 Dec | a whole show of performers, spares and `LOST`; `tests/check.sh phase3 p3_basic p3_spare` |
| Tue 8 Dec | orphans, rescue, `LAND`, a lossy radio, `Ctrl-C` everywhere; `make check3` |
| Thu 10 Dec | two machines, the load measurement, your tests, `DESIGN.md`, `REPORT.md` |

---

## 🛠️ Task 1: TCP and UDP Behind the Same API (20%, checkpoint 3 December)

Add two schemes to `chan/` so that `make test3` (`bin/test_chan tcp udp`) passes, and `make test2`
still does: `make test` runs all five.

- **`tcp:HOST:PORT`** — the line rules of `local:` (PROTOCOL.md 1.1) over TCP. `HOST` may be a name
  or a number (`getaddrinfo`); `tcp::PORT` listens on every address; port 0 picks a free port, and
  `chan_addr(c, CHAN_LOCAL, ...)` tells which. `chan_open` gives up after a few seconds when the
  host does not answer, instead of the system's two minutes.
- **`udp:HOST:PORT`** — one message, one datagram (at most 512 bytes). `chan_open` talks to one
  address, and reports `ECONNREFUSED` from the system as `CHAN_CLOSED`. `chan_listen` receives from
  anybody, and its `chan_send` answers the last sender (the reply rule of 1.2).
- **`SWARM_BIND`** and **`SWARM_DROP`** (PROTOCOL.md 1.2, 1.5): the tests run every program on its
  own `127.0.0.x`, and make the radio lose datagrams.

**In `REPORT.md`:** send 100,000 datagrams as fast as you can to a receiver that sleeps 1 ms after
each one, with `SWARM_DROP` unset. How many arrive, where do the others go, and how do you know
(`ss -uam`, `netstat -su`)? What does this mean for the radio?

## 🌐 Task 2: The Center on the Network (20%)

The center of Phase 2 with `--listen tcp::PORT`, plus the callers of PROTOCOL.md 3.4:

- **`JOIN`**: a performer gets a free slot of a captain that has a radio: `REHOME` to the performer,
  `ADOPT` to the captain. With no free slot, it is a **spare**: its connection stays open until a
  slot frees up (a `LOST`) or the show ends (`LAND -1`).
- **`ORPHANED`**: `REHOME` to whoever flies that zone now, with `ADOPT`; or `LAND` if nobody does.
- **The start rule** (PROTOCOL.md 5): every zone has a captain *and every radio slot a performer*,
  or `--wait` seconds have passed.
- **Text frames over TCP** to every `WATCH` viewer, on any machine. A viewer whose network is slow
  gets fewer frames, never an old backlog, and never slows the beat.
- **Nothing blocks the center**: a caller that connects and says nothing, a viewer that stops
  reading (`kill -STOP` it), a hundred connections at once, a spare that waits for the whole show.

**In `REPORT.md`:** your concurrency model (a thread per connection, a `poll` loop, or a mix) and
why; and what happens, buffer by buffer, when a viewer on another machine stops reading.

## 📡 Task 3: The Radio and the Performer (30%)

**The captain** with `--radio udp::0` opens its radio first, says `HELLO <its radio port>`, and
flies performers instead of its own drones:

- A slot is empty until the center sends `ADOPT f` and a performer says `NEXT f ...` from a new
  address. From then on only that address speaks for the slot (anybody else gets `LAND`), until the
  next `ADOPT f`.
- On `BEAT b` it plans every drone's move as in Phase 2, and answers each `NEXT` with its newest
  `GOTO` for that slot, or `WAIT`. It never sends first and keeps no resend timers: the performers'
  questions drive the radio.
- **`POS` is the truth** (PROTOCOL.md 3.2): it reports a drone's new cell only after its performer
  confirmed it (`NEXT f b+1`). A drone whose move is not confirmed yet may be in either cell, so it
  holds **both** until it is: no other drone may move into either one.
- `PERFORMER_LOST_MS` (3 s) without a word from a slot that has a move waiting: `LOST f` to the
  center, and the slot is empty again.
- `SHUTDOWN`: half a second of answering every `NEXT` with `LAND`, then exit (PROTOCOL.md 5).

**The performer** (`bin/performer --center ADDR`) is one drone: `JOIN`, then the radio, then
`LAND`. It asks `NEXT` every `RADIO_RETRY_MS` (50 ms) while it waits, ignores duplicates and stale
answers, becomes an orphan after `ORPHAN_MS` (1.5 s) of silence (or `CHAN_CLOSED`), asks the center
`ORPHANED`, and exits when told `LAND` or when the center is gone too. It can be small: one thread,
one loop.

**In `REPORT.md`:** a timeline of one beat of one drone, from `tcpdump -i lo -n udp`, in which a
`GOTO` is lost and then a `NEXT` is lost. Explain why each loss cannot put two drones in one cell.

## 🚑 Task 4: Failures and Signals (15%)

Each of these must end the show normally, with no collision and no speed violation, and leave
nothing behind:

| Failure | What must happen |
|---|---|
| `kill -9` a performer | its captain reports `LOST` after 3 s; a spare takes the slot (`tests/check.sh phase3 p3_spare`) |
| `kill -9` a captain, or `swarmctl kill`, with a standby | the standby takes the zone; every orphan is rehomed to it (`p3_rescue`) |
| the same with no standby | the zone stays frozen; every orphan is told `LAND` and exits (`p3_land`) |
| `kill -STOP` a captain for 5 s, then `kill -CONT` | after 1 s the center declares it dead and closes its connection; the standby takes over; when it continues, the old captain finds its connection closed and exits |
| `SWARM_DROP=20` on every captain and performer | the show still ends, and the picture still forms (`p3_lossy`) |
| `Ctrl-C` (`SIGINT`) or `SIGTERM` to the center | every captain gets `SHUTDOWN` and lands its performers; spares get `LAND -1`; everything exits within 3 s |
| `Ctrl-C` to a captain, a performer, a viewer | a captain death, a lost drone, a viewer gone: the show goes on |

After every run: `pgrep -fa 'center|captain|performer|viewer'` and `ss -tuanp` show nothing of yours.

**In `REPORT.md`:** the timeline of one rescue from your logs (the kill, "dead", the takeover, the
first and the last `REHOME`, the zone's first move again). Then the false alarm: stop a captain
with `SIGSTOP` for 0.8 s, and for 1.2 s. What happens in each case? On a Wi-Fi network with 300 ms
spikes, what would you set `CAPTAIN_DEAD_MS` and `ORPHAN_MS` to, and what would that cost?

## 📈 Task 5: Two Machines, Load, Tests, Report (15%)

- **`make check3`** passes; `make test` and `make check2` still pass.
- **Two machines.** The center on one machine, at least one captain and its performers on another;
  a viewer on a third if you can. Kill the remote captain and watch its performers be rehomed
  across the network. Put the commands and a screenshot in the report.
- **Load.** Write `tests/load.sh`: the long score (`data/scores/long.score`) at tempo 0, on one
  machine, printing one row per run: drones, beats, seconds, beats/s, `BUSY` replies, late beats,
  datagrams per second, collisions and speed violations (both 0). Change **one** thing at a time
  (followers per captain, number of captains, `SWARM_DROP`, your planner's workers or queue size,
  `RADIO_RETRY_MS` in a copy of `swarm.h`) and put the rows in a table. Explain the shape with
  evidence (`top -H`, `perf top`, `strace -c -f`, timestamps in your logs), not guesses.
- **Two tests of your own** in `tests/` (for example: a viewer that joins in the middle of the show;
  1000 operator commands during a tempo-0 show; 30% loss plus a captain kill; 20 spares).
- **`DESIGN.md`** updated: every process and thread, every lock and its order, and what each
  message makes happen. **`REPORT.md`**: the answers asked in Tasks 1-4, the load table and your
  explanation, what you would change for 10,000 drones, and the AI assistance you used (what you
  asked, what you kept, what you had to fix).

---

## 🎬 Final Demo (in the lab, about 8 minutes)

Perform the heart show (`data/scores/show.score`, 4 captains × 46 performers) or a score of your
own, with a captain or a viewer on a second machine if you can. Live: kill a captain with a standby
waiting, freeze another (`kill -STOP`), kill a few performers with spares waiting, and let the
picture form anyway. Then `Ctrl-C` the center and show that nothing is left (`pgrep`, `ss`), and
replay the recording. Be ready to explain any line of your code.

## 🧯 Hints

- `ss -tlnp` shows who listens on which port, `ss -tnp` the TCP connections, `ss -uanp` the UDP
  sockets. A test that crashed may leave a center listening: `pkill -f 'center --listen'`.
- `strace -f -e trace=network bin/captain ...` shows every socket call; `-e trace=signal` every
  signal.
- `tcpdump -i lo -n -A udp` (or Wireshark) shows the radio; with `SWARM_DROP` you see the questions
  repeat.
- "Address already in use" after a restart: `SO_REUSEADDR` before `bind`.
- A connected UDP socket reports a dead peer (ICMP "port unreachable") **on the next call**, `send`
  or `recv`, whichever comes first. Check both.
- On a UDP server, `chan_recv` and `chan_addr(c, CHAN_REMOTE, ...)` belong in the same thread with
  nothing in between: the "last sender" changes with every datagram.
- A non-blocking `connect` and `poll` with a deadline is how `chan_open` gives up early; then read
  the real result with `getsockopt(SO_ERROR)`.
- A signal handler may only set a `volatile sig_atomic_t` flag (or write a byte to a pipe): no
  `printf`, no `malloc`, no locks. Every wait in your code needs a timeout so that the flag is
  seen.
- Lab machines may filter ports: ask for one, or try `nc -l 9000` on one machine and
  `nc HOST 9000` on the other first. WSL2 sits behind NAT: other machines cannot reach a captain
  inside it.
- `tests/logs/<scenario>/` keeps every program's output. Read the center's `.err` first.

## 🚀 Bonus (up to +15%)

- B1: **Fallback routine.** A captain that gets `BUSY` for too long, or loses the center, does not
  freeze its troupe: it switches to a built-in routine (hold the formation and pulse the color on
  its own clock) and keeps asking for a real plan with randomized exponential backoff.
- B2: **Center restart.** Captains and performers survive a center crash: they reconnect with
  backoff, and the restarted center rebuilds the show from what they tell it. Write down the
  messages you add (as a section of your own after PROTOCOL.md's).
- B3: **A booth per captain, on its machine.** `bin/viewer --from tcp:A:9000 --captain 2` on
  captain 2's machine: its zone, its performers, its own controls (and Qt 6 or GTK 4, if you did not
  do Phase 2's B2).
- B4: **A web viewer.** The center (or a small gateway program of its own) speaks WebSocket, and a
  browser shows the show.
- B5: **A real model behind the planner** (an LLM or a planner service over HTTP), with timeouts and
  `BUSY` when it is slow. The protocol does not change.
- B6: **The other server model** (Task 2): `poll` or `epoll` instead of threads (or the reverse),
  with a measured comparison at 100 viewers.
- B7: **A view API.** `WATCH` pushes every frame to a viewer. Add the other style, request and
  response: a program asks the center a question and gets the coordinates of the drones it asked
  for, now. A dashboard, a script or a scoring service needs this, not a stream. On the same port,
  a new first line:

  ```text
  QUERY                 the first line: questions follow, one per line; the connection stays open
  DRONES            →   DRONES <beat> <n>, then n lines  P <id> <zone> <x> <y> <color> <state>
  DRONE <id>        →   P <id> <zone> <x> <y> <color> <state>   (or ERR no such drone)
  CAPTAIN <zone>    →   DRONES <beat> <k>, then the k drones of that zone
  anything else     →   ERR bad request
  ```

  `<state>` is a word (`ok`, `lost`). The answers come from the newest frame the center published:
  copy it under a short lock and answer from the copy, so a question never takes the center's own
  locks or waits for a beat, and a client that asks and never reads is dropped like a slow viewer.
  Give `swarmctl` a client (`swarmctl --center tcp:A:9000 drones [ID | --captain Z]`); `nc` works
  too: `printf 'QUERY\nDRONE 17\n' | nc -q1 127.0.0.1 9000`. Show in your tests that 1000 questions
  during a tempo-0 show do not slow it down, and that every answer matches the recorded frame of its
  beat. Going further: answer the same questions over HTTP with JSON (`GET /drones`,
  `GET /drones/17`, `GET /captains/2/drones`), so `curl` and a browser (B4) can use it.

## ✅ Grader's Checklist

```bash
make && make test && make check2 && make check3                      # all passed
SWARM_BIND=127.0.0.2 bin/center --listen tcp::9000 --score data/scores/show.score \
                     --captains 4 --followers 46 --record file:/tmp/g.rec &
for i in 11 12 13 14 15; do SWARM_BIND=127.0.0.$i bin/captain --center tcp:127.0.0.2:9000 --radio udp::0 & done
for i in $(seq 1 190); do SWARM_BIND=127.0.1.$i bin/performer --center tcp:127.0.0.2:9000 & done  # 6 spares
SWARM_BIND=127.0.0.50 bin/viewer --from tcp:127.0.0.2:9000 --gui &
bin/swarmctl --center tcp:127.0.0.2:9000 kill 2          # the standby takes zone 2, orphans rehomed
kill -9 $(pgrep -f bin/performer | head -3)               # spares take their slots
kill -STOP $(pgrep -o -f bin/captain)                     # zone 0 freezes, its orphans land
kill -INT %1; sleep 3; pgrep -fa 'center|captain|performer|viewer'; ss -tuanp | grep 127.0   # nothing left
bin/framecheck /tmp/g.rec && bin/viewer --from file:/tmp/g.rec --gui
```

Zero-tolerance items: a program that starts another one; a center that a viewer, an operator, a
spare or a silent caller can block; a collision or a speed violation; a process left running after
the show or after `Ctrl-C`; a signal handler that calls `printf` or `malloc`; busy-waiting or
`sleep`-and-check loops; a missing `DESIGN.md` or report.
