# Final Project: Drone Swarm Painters — A Leader/Follower Swarm Drawing Through an Inference Server

**Course:** BIL 322 System Programming
**Topic:** Processes, threads, synchronization, IPC, shared memory, sockets, signals — all in one system
**Mode:** Pairs (individual allowed; expectations are the same)
**Duration:** 3 phases, about 1 week each (details of each phase are refined when it is assigned)
**AI use:** Allowed, but must be disclosed and you must be able to explain all code

> **Status: DRAFT.** Structure, protocol, and grading are stable. Each week, the
> instructor may adjust details of the phase being assigned and release the
> skeleton code with it.

---

## 1. Story

A relief organization wants a drone show that is also a message: a swarm of
drones that **paints a picture** on a huge ground canvas (a stadium-sized grid)
— a logo, a map, a flag, a portrait.

The swarm has a command structure:

- One **Orchestrator Center** — an inference-style server. It knows the target
  image, splits it into regions, and answers *planning requests* from leaders.
- Several **Leader drones** — the *agents*. Each is assigned a region. A leader
  asks the center to plan its region ("which cells in what order?") and then
  commands its followers. In a real system the planner would be an LLM; in this
  course the planner is a **stub with simulated latency** behind a small
  interface (an agent does not have to mean an LLM, and nobody needs a GPU).
- Many small **Follower drones** — cheap workers. Each flies to a cell, paints
  it, and asks its leader for the next one.
- A **Viewer** — an independent GUI (SDL, GTK, or Qt: your choice) that shows
  the canvas and the drones moving. It must never be able to slow the swarm down.

Why this project:

| Course topic | Where it appears |
|---|---|
| fork / exec / wait, pipes (Weeks 4-5) | Center launches leaders and talks to them |
| Memory pools (Week 6, Lab 6) | The center's per-session KV pools for planning requests |
| Threads, mutexes, condition variables, semaphores | Followers, task queues, request queue, thread pool |
| Deadlock, livelock, starvation | Two followers swapping targets, full queues, retry storms |
| Shared memory, `mmap` (Week 12) | Shared canvas, viewer as a separate process |
| Sockets (Week 13) | Every link in the system in the final phase |
| Signals (Week 14) | Graceful shutdown, leader crash handling |

---

## 2. The Big Picture

```text
                     ┌───────────────────────────────────────┐
                     │           ORCHESTRATOR CENTER          │
                     │  target image · region splitter        │
                     │  request queue → planner thread pool   │
                     │  (each request gets a KV arena, Lab 6) │
                     └───────┬───────────────────┬───────────┘
                  plan/assign│                   │ canvas state
                             ▼                   ▼
                ┌─────────────────┐        ┌──────────┐
                │  LEADER drone   │  ...   │  VIEWER  │  (separate GUI,
                │  (agent)        │        │  GUI     │   read-only)
                └───┬───────┬─────┘        └──────────┘
          commands  │       │
              ┌─────▼──┐ ┌──▼─────┐
              │follower│ │follower│  ...  (cheap, many)
              └────────┘ └────────┘
```

**One protocol, three transports.** The same line-based text messages (Section 3)
travel over *pipes* in Phase 1, *shared memory + pipes* in Phase 2, and *TCP
sockets* in Phase 3. If you write your send/receive code against a file
descriptor and keep protocol parsing separate from the transport, Phase 3
is a small change, not a rewrite. That is the main design lesson of the project.

### What runs as what

| | Phase 1 | Phase 2 | Phase 3 |
|---|---|---|---|
| Center | parent process | parent process | server process |
| Leader | child process (`fork`/`exec`) | child process | client process (own socket) |
| Follower | **thread** inside its leader | thread inside its leader | thread inside its leader, **talks to its leader over a socket** |
| Viewer | window inside the center | **separate process**, reads shared memory | separate process, socket client |
| Center ↔ leader | pipes | pipes + shared canvas | TCP |

In Phase 3 *everything communicates through sockets* — center↔leader,
leader↔follower, and viewer↔center. Followers stay threads inside the leader
process (a follower thread owns one socket connection to its leader), so a
follower could later be moved into its own process without changing its code.

---

## 3. The Protocol (Draft)

One message per line, fields separated by single spaces, `\n` terminated,
maximum 256 bytes per line. Unknown messages are ignored and logged, never fatal.

### Leader → Center

```text
HELLO <leader_id> <n_followers>
PLAN_REQ <req_id> <region_id>            ask the planner for a painting order
PAINTED <leader_id> <x> <y> <color>      a follower finished one cell
DONE <leader_id> <region_id>
HEARTBEAT <leader_id>
```

### Center → Leader

```text
ASSIGN <region_id> <x0> <y0> <x1> <y1>   your region (inclusive corners)
PLAN <req_id> <n>                        followed by n lines: "<x> <y> <color>"
BUSY <req_id> <retry_ms>                 the request queue is full; retry later
SHUTDOWN
```

### Leader ↔ Follower (Phase 3 only; in Phases 1-2 these are function calls and queues)

```text
NEXT <follower_id>                       follower asks for work
GOTO <x> <y> <color>                     leader hands out one cell
PAINTED <follower_id> <x> <y>
```

### Center → Viewer (Phase 3)

```text
FRAME <seq> <w> <h>                      followed by h lines of w color digits
DRONE <id> <x> <y> <role>                one line per drone, after the frame
```

The viewer only needs to *eventually* show the latest state. If it is slow, it
misses frames. It must never block the center.

---

## 4. Phases

### Phase 1: One Machine, Leaders as Processes, Followers as Threads

**After:** Week 7-8 lectures (threads, synchronization)
**Details:** see [`labs/swarm-phase1-threads-sync.md`](../labs/swarm-phase1-threads-sync.md)

- The center forks one leader process per region (your Lab 4 code, reused) and
  talks to each over a pair of pipes using the protocol above.
- Each leader runs follower **threads** that pull cells from a thread-safe task
  queue and "fly" there.
- The center owns the canvas. Followers report `PAINTED`; the center applies
  it under a lock and a simple SDL view (inside the center) draws it.
- The center's planner is a thread pool fed by a bounded request queue, with a
  per-request arena from your Lab 6 pool. A full queue answers `BUSY`.
- The lab explores **lock granularity** on the canvas and the one deadlock the
  skeleton deliberately allows you to find.

### Phase 2: Shared Canvas and an Independent Viewer

**After:** Week 12 (virtual memory and IPC)
**Details:** `labs/swarm-phase2-ipc-shared-canvas.md`

- Move the canvas into `mmap`ed **shared memory** with process-shared
  synchronization, so leaders can paint directly and the center no longer funnels
  every pixel.
- The viewer becomes a **separate process** that maps the canvas read-only. A
  frame counter lets it detect updates without blocking writers.
- Pipes remain for planning requests and assignments.
- Collision rule: two followers must not paint the same cell at the same time.
  Decide who prevents it and prove it with a test.

### Phase 3: Everything Over Sockets, Hardened

**After:** Week 13-14 (sockets, signals)
**Details:** `labs/swarm-phase3-sockets-signals.md`

- Replace pipes and shared memory with **TCP**. The center is a multithreaded
  server; leaders are clients; followers talk to their leader over sockets; the
  viewer is a socket client that receives `FRAME` messages.
- **Signals:** `SIGINT` shuts the whole swarm down gracefully; `SIGPIPE` is
  handled; a leader killed with `SIGKILL` is detected (heartbeat timeout) and its
  region is reassigned.
- **Load test:** one command starts 1 center, 4+ leaders, 50+ followers and
  paints the target image. Report time, `BUSY` count, and accuracy.

---

## 5. The Planner Interface (the "LLM")

The center never calls a model directly. It calls one function, so a real model
can be plugged in as a bonus:

```c
/* planner.h — given. Do not change the signature. */
typedef struct {
    int x, y;
    unsigned char color;
} Cell;

/* Fill out[] with up to max cells for region r of the target image, in the
 * order they should be painted. Returns the number of cells written, or -1.
 * The default implementation sleeps for a simulated "thinking time" that grows
 * with the region size, then returns a row-major or nearest-neighbor order. */
int plan_region(const Image *target, const Region *r, Cell *out, int max);
```

The simulated latency is what makes queueing, thread pools, `BUSY` responses,
and timeouts matter. Bonus: replace the stub with an HTTP call to a local or
remote model, behind the same function.

---

## 6. Constraints

- C only (the viewer may use SDL2, GTK, or Qt).
- Follow the [secure C rules](../labs/secure-c-programming-rules-for-assignments.md).
- No busy-waiting. Use condition variables, semaphores, or blocking I/O.
- Valgrind (and ThreadSanitizer for Phases 1-2) must be clean on the headless test
  mode (`--no-gui`). Every program must run without a display.
- Every protocol parser must reject malformed and over-long lines without
  crashing.

---

## 7. Grading (Draft)

| Phase | Weight |
|---|---|
| Phase 1: threads, queues, canvas locking, report | 30% |
| Phase 2: shared memory, independent viewer, collision test | 30% |
| Phase 3: sockets, signals, fault handling, load test | 30% |
| Final demo and report | 10% |
| Bonus (real model behind the planner, swarm formations, nicer visuals) | up to +15% |

Accuracy of the painted image (fraction of cells matching the target) is
reported in each phase but is not graded directly; correctness of the
concurrency is.

---

## 8. What We Provide vs. What You Build

| Provided (per phase, with the skeleton code) | You build |
|---|---|
| Protocol parser tests, `planner.h` and the stub planner | Transport layer, queues, canvas, drones |
| Image loader (PBM/PPM) and region splitter | Leader and follower logic |
| Starter SDL viewer | Viewer changes for each phase |
| Test images and a headless test harness | Your own tests and the report |

You may reuse your Lab 4 orchestrator, Lab 5 pipes, and Lab 6 pool. Earlier
years' drone simulator (`2025-bil222/projects/emergency-drone-coordination-system`)
has a thread-safe list and an SDL view you may study; it is a different
architecture, so do not copy it wholesale.

---

## 9. Report Requirements

Each phase: a short `REPORT.md` with the architecture sketch, the locking
scheme (what each mutex protects and the lock ordering), one race or deadlock you
hit and how you found it, and the AI assistance you used.
