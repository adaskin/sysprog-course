# Final Project: 19 Mayıs Drone Show — A Swarm Choreographed by Agents

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

It is the 19th of May, Commemoration of Atatürk, Youth and Sports Day. The
stadium is full. Between the student choreographies, the night sky is the stage
for a **drone show set to music**.

The organizers do not want one big drone formation moving as a block. They want
**semi-independent groups of drones**, each performing its own part of the
show, all locked to the same music:

- One **Orchestrator Center** reads the musical score and decides the **coarse
  theme** of each section: *"bars 1-16: dawn, calm, sweeping lines of warm light;
  bars 17-32: the flag, energetic."*
- Several **Leader drones** — the *agents* — each own a **zone** of the sky. A leader
  takes the theme, the mood, and the beat grid, and invents the **fine detail**
  for its group: who moves where on which beat, in what color, with what
  variation. Two leaders given the same theme should produce *different* but
  compatible performances.
- Many small **Follower drones** — cheap light points. Each follows its leader's
  keyframes beat by beat.
- A **Viewer** — an independent GUI (SDL, GTK, or Qt: your choice) showing the sky.
  It never controls anything and must never slow the swarm down.

Why agents? Following a fixed script is easy. Deciding what several dozen drones
should *do* with a mood and a zone, under speed and collision limits, in time for
the next section, is an open-ended planning problem. That is the job of an agent
(in the real world: an LLM). In this course the default agent is a **deterministic
stub with simulated thinking time** behind a small interface (an agent does not
have to mean an LLM, and nobody needs a GPU or an API key).

### The contract: a fixed protocol

The messages (Section 3) are **frozen**. They are plain text, one per line, so that
*agents and humans can both follow them*: a student can type a message with `nc`
to test a leader by hand, and an LLM can be given the protocol text as its
instructions. If your system only works with your own code on the other side, it is
not done.

### Why this project

| Course topic | Where it appears |
|---|---|
| fork / exec / wait, pipes (Weeks 4-5) | Center launches leaders and talks to them |
| Memory pools (Week 6, Lab 6) | The center's inference service gives every planning request a KV arena |
| Threads, mutexes, condition variables, semaphores, barriers | Followers moving in lockstep, task and request queues, thread pool |
| Deadlock, livelock, starvation | Two followers swapping places, full queues, retry storms |
| Shared memory, `mmap` (Week 12) | The shared stage state, viewer as a separate process |
| Sockets (Week 13) | Every link in the system in the final phase |
| Signals (Week 14) | Graceful shutdown, leader crash handling |

---

## 2. The Big Picture

```text
                     ┌───────────────────────────────────────┐
                     │           ORCHESTRATOR CENTER          │
                     │  score (music) · beat clock · themes   │
                     │  inference service: request queue →    │
                     │  planner pool (KV arena per request)   │
                     └───────┬───────────────────┬───────────┘
        THEME, BEAT, PLAN    │                   │ stage state
                             ▼                   ▼
                ┌─────────────────┐        ┌──────────┐
                │  LEADER drone   │  ...   │  VIEWER  │  (separate GUI,
                │  (agent, zone)  │        │  GUI     │   read-only)
                └───┬───────┬─────┘        └──────────┘
        keyframes   │       │
              ┌─────▼──┐ ┌──▼─────┐
              │follower│ │follower│  ...  (cheap light points)
              └────────┘ └────────┘
```

### The stage and the clock

- The sky is a top-down grid (default 128 × 72 cells). Every drone occupies one cell
  and shows one color (0-15).
- Time is counted in **beats**. A **score** file (given) lists sections:

```text
# beat  section  theme   mood
0       intro    dawn    calm
32      march    flag    energetic
64      finale   number19 triumphant
```

- Between beats a follower may move **at most 2 cells**. No two drones may be in the same
  cell on the same beat. Both rules are checked by a given validator; a leader
  must not send a plan that breaks them.
- The beat clock runs at the tempo in the score header. Tests run at "tempo 0" (as fast
  as the system can go), so the logic must be correct, not just timed right.

### One protocol, three transports

The same messages travel over *pipes* in Phase 1, *pipes + shared memory* in Phase 2,
and *TCP sockets* in Phase 3. If you write your send/receive code against a file
descriptor and keep protocol parsing separate from the transport, Phase 3 is a
small change, not a rewrite.

| | Phase 1 | Phase 2 | Phase 3 |
|---|---|---|---|
| Center | parent process | parent process | server process |
| Leader | child process (`fork`/`exec`) | child process | client process (own socket) |
| Follower | **thread** inside its leader | thread inside its leader | thread inside its leader, **talks to its leader over a socket** |
| Viewer | window inside the center | **separate process**, reads shared memory | separate process, socket client |
| Center ↔ leader | pipes | pipes + shared stage | TCP |

In Phase 3 *everything communicates through sockets* — center↔leader,
leader↔follower, and viewer↔center. Followers stay threads inside the leader
process (a follower thread owns one socket connection to its leader), so a follower
could later be moved into its own process without changing its code.

---

## 3. The Protocol (Draft, Frozen Once Phase 1 Is Assigned)

One message per line, fields separated by single spaces, `\n` terminated, maximum
256 bytes per line. Unknown messages are ignored and logged, never fatal.

### Center → Leader

```text
ASSIGN <leader_id> <x0> <y0> <x1> <y1> <n_followers>   your zone (inclusive corners)
THEME <section> <start_beat> <end_beat> <theme> <mood> coarse direction for a section
PLAN <req_id> <n>                                      followed by n keyframe lines
BUSY <req_id> <retry_ms>                               the inference queue is full
BEAT <n>                                               the clock (Phase 3; earlier: local)
SHUTDOWN
```

### Leader → Center

```text
HELLO <leader_id> <n_followers>
PLAN_REQ <req_id> <section> <style_seed>               ask the inference service for detail
READY <leader_id> <section>                            positions for this section are loaded
HEARTBEAT <leader_id>
DONE <leader_id> <section>
```

### Keyframe line (inside `PLAN`, and also what a leader stores)

```text
<beat> <follower_idx> <x> <y> <color>
```

"At `beat`, follower `follower_idx` is at `(x, y)` showing `color`."

### Leader ↔ Follower (Phase 3 only; in Phases 1-2 these are function calls and queues)

```text
NEXT <follower_id> <beat>                follower asks for its target for this beat
GOTO <x> <y> <color>                     leader's answer
AT <follower_id> <beat> <x> <y>          follower reports where it is
```

### Center → Viewer (Phase 3)

```text
FRAME <beat> <w> <h>                     followed by h lines of w color digits
DRONE <id> <x> <y> <role>                one line per drone, after the frame
```

The viewer only needs to *eventually* show the latest state. If it is slow, it misses
frames. It must never block the center.

---

## 4. Phases

### Phase 1: One Machine, Leaders as Processes, Followers as Threads

**After:** Week 7-8 lectures (threads, synchronization)
**Details:** see [`labs/swarm-phase1-threads-sync.md`](../labs/swarm-phase1-threads-sync.md)

- The center forks one leader process per zone (your Lab 4 code, reused) and talks
  to each over a pair of pipes using the protocol above.
- Each leader runs follower **threads** that advance in lockstep, one beat at a time
  (a barrier), toward their keyframes.
- The center's inference service is a thread pool fed by a bounded request queue, with
  a per-request arena from your Lab 6 pool. A full queue answers `BUSY`.
- The center owns the stage; an occupancy grid enforces "one drone per cell". A simple
  SDL view lives inside the center.
- The lab explores **lock granularity** on the occupancy grid and the one deadlock the
  skeleton deliberately allows you to find.

### Phase 2: Shared Stage and an Independent Viewer

**After:** Week 12 (virtual memory and IPC)
**Details:** `labs/swarm-phase2-ipc-shared-stage.md`

- Move the stage state (beat, drone positions and colors) into `mmap`ed **shared
  memory** with process-shared synchronization, so leaders update it directly.
- The viewer becomes a **separate process** that maps the stage read-only.
- Pipes remain for themes, planning requests, and plans.
- Collision rule: who prevents two drones from taking the same cell? Decide and prove it.

### Phase 3: Everything Over Sockets, Hardened

**After:** Week 13-14 (sockets, signals)
**Details:** `labs/swarm-phase3-sockets-signals.md`

- Replace pipes and shared memory with **TCP**. The center is a multithreaded server;
  leaders are clients; followers talk to their leader over sockets; the viewer is a
  socket client that receives `FRAME` messages.
- **Signals:** `SIGINT` shuts the whole show down gracefully; `SIGPIPE` is handled; a
  leader killed with `SIGKILL` is detected (heartbeat timeout) and its zone is taken
  over, so the show goes on.
- **Load test:** one command starts 1 center, 4+ leaders, 50+ followers, and performs the score.

---

## 5. The Agent Interface (the "LLM")

Leaders do their thinking through the center's inference service. The service calls
one function, so a real model can be plugged in as a bonus:

```c
/* planner.h — given. Do not change the signature. */
typedef struct {
    int beat;
    int follower;       /* index within the leader */
    int x, y;
    unsigned char color;
} Keyframe;

/* Fill out[] with up to max keyframes that make n_followers drones perform
 * `theme` in `mood` inside `zone` from start_beat to end_beat. style_seed makes
 * different leaders produce different (but valid) variations.
 * Returns the number of keyframes written, or -1.
 * The default implementation sleeps for a simulated "thinking time" that grows
 * with the zone and section length, then returns a formation sequence (lines,
 * circles, waves, the digits "19") chosen by theme and mood. */
int plan_section(const Theme *theme, const Zone *zone, int n_followers,
                 unsigned style_seed, Keyframe *out, int max);
```

The simulated latency is what makes queueing, thread pools, `BUSY` responses and timeouts
matter. **Leaders must check every plan with the given validator** (speed, collisions,
zone) and ask again — or repair it — if it fails: a real model would sometimes get it
wrong. Bonus: replace the stub with an HTTP call to a local or remote model behind the
same function, or write a plan by hand and feed it through the protocol.

---

## 6. Constraints

- C only (the viewer may use SDL2, GTK, or Qt).
- Follow the [secure C rules](../labs/secure-c-programming-rules-for-assignments.md).
- No busy-waiting. Use condition variables, semaphores, barriers, or blocking I/O.
- Valgrind (and ThreadSanitizer for Phases 1-2) must be clean on the headless test
  mode (`--no-gui`). Every program must run without a display.
- Every protocol parser must reject malformed and over-long lines without crashing.

---

## 7. Grading (Draft)

| Phase | Weight |
|---|---|
| Phase 1: threads, queues, barrier, occupancy grid, report | 30% |
| Phase 2: shared memory, independent viewer, collision test | 30% |
| Phase 3: sockets, signals, fault handling, load test | 30% |
| Final demo and report | 10% |
| Bonus (real model behind the planner, new themes, nicer visuals) | up to +15% |

The **performance score** (fraction of keyframes reached on their beat, and the number of
safety violations, which must be zero) is reported in each phase; correctness of the
concurrency is what is graded.

---

## 8. What We Provide vs. What You Build

| Provided (per phase, with the skeleton code) | You build |
|---|---|
| Score parser, `planner.h`, the stub planner and its formations | Transport layer, queues, stage, drones |
| The plan validator (speed, collision, zone) | Leader and follower logic |
| Starter SDL viewer | Viewer changes for each phase |
| Test scores and a headless test harness | Your own tests and the report |

You may reuse your Lab 4 orchestrator, Lab 5 pipes, and Lab 6 pool. Earlier years' drone
simulator (`2025-bil222/projects/emergency-drone-coordination-system`) has a thread-safe
list and an SDL view you may study; it is a different architecture, so do not copy it
wholesale.

---

## 9. Report Requirements

Each phase: a short `REPORT.md` with the architecture sketch, the locking scheme (what
each mutex protects and the lock ordering), one race or deadlock you hit and how you found
it, and the AI assistance you used.
