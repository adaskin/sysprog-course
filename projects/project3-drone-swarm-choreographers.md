# Final Project: Autonomous Drone Swarm Choreographers

**Course:** BIL 322 System Programming
**Topic:** Processes, threads, synchronization, IPC, shared memory, sockets, signals — all in one system
**Mode:** Pairs (individual allowed; expectations are the same)
**Duration:** 3 phases, about 1 week each (details of each phase are refined when it is assigned)
**AI use:** Allowed, but must be disclosed and you must be able to explain all code

> **Status: Phase 1 is final** (the protocol of Section 3 is frozen, and the Phase 1 skeleton
> is in [`labs/swarm-phase1-skeleton/`](../labs/swarm-phase1-skeleton/)). Phases 2 and 3 are
> drafts: the instructor adjusts their details, and releases their skeleton code, in the week
> each one is assigned.

---

## 1. Story

**Every year, the stadium fills with color.**

At the big stadium festival, students pack the stands and perform mass choreographies:
hundreds of them raise colored cards in perfect timing to paint giant mosaics on the
field — a flag, a logo, a word — all synchronized to music.

This year, the engineering faculty is taking the tradition into the air. Above the
stadium, a swarm of small LED drones will perform alongside the students: a living
mosaic of glowing drones that moves to the music, flows from one formation to the next,
and in the finale assembles a picture in the sky. Your team is building the swarm.

- **The Orchestrator Center — the Artistic Director.** It reads the **score** (the
  music's sections and moods) and holds the **target image** the finale must form. It
  decides the *coarse* theme of every section, splits the stage into zones, and assigns
  each troupe its part. It also hosts the **choreographer**: a slow, busy thinker that
  turns a theme into detailed movements. In a real production the choreographer would be
  an LLM; in this course it is a stub with simulated latency behind a small interface,
  because nobody needs a GPU to learn what it feels like to wait on a model that is slow,
  busy, or wrong.
- **The Leader drones — the Dance Captains.** Each captain owns one zone — one troupe.
  The center sets the theme; the captain drives the performance. A leader commissions
  the detailed choreography for its zone from the choreographer (each captain with its
  own *style*, so two troupes given the same theme dance differently but compatibly),
  checks it, and directs its performers through it. Troupes are semi-independent: each
  is its own small show inside the whole.
- **The Follower drones — the Performers.** Cheap, numerous, glowing. Each flies to a
  position, lights its color, and radios its captain: *"Next."* Performers don't
  choreograph. They dance.
- **The Viewer — the Broadcast Booth.** An independent GUI that shows the sky for the
  crowd on the stadium screens. It must never make a performer hesitate: if the
  broadcast lags, it drops frames — never the swarm.

**Why agents and not one central script?** The choreography is too large and too fast
for a single director to micromanage a hundred drones, and deciding what dozens of
drones should do from a mood and a zone, under speed and collision limits, is an
open-ended planning problem. The show only works if it is split into semi-independent
troupes that manage their own corner, react in real time, and keep dancing even when the
director is busy. That is exactly the leader/follower architecture — and exactly why
this course is about processes, threads, queues, and fault tolerance.

**Why the protocol is fixed and human-readable:** every message is one plain-text line,
so that not only the agents, but also the student operators in the control booth, can
watch the rehearsal scroll by, follow the swarm's reasoning, and step in when a troupe
goes off-script. A student can type a message with `nc` to test a captain by hand, and
an LLM can be given the protocol text as its instructions. A protocol only machines can
read is a protocol you can't debug on show night.

![What the swarm does: home formation, waves, the mosaic forms, the final picture](../labs/images/swarm/swarm-show.png)

*A Phase 1 run, drawn from the center's ASCII output: 4 captains with 46 performers each. The
Phase 2 viewer draws the same thing in a real window.*

You have one swarm, one battery budget, and the length of one song. The picture either
forms by the final note, or it doesn't.

**The question threaded through every phase:** when a captain's battery dies
mid-performance, its troupe is suddenly *orphaned* — still flying, still glowing, still
asking *"Next?"* of a captain that will never answer. Who notices? Who takes over the
troupe? That question is the heart of this project, and it gets harder every phase.

### Why this project

| Course topic | Where it appears |
|---|---|
| fork / exec / wait, orphans (Weeks 4-5) | Center launches captains; a dead captain must be noticed and its troupe rescued |
| Pipes (Week 5) | Phase 1 links |
| Memory pools (Week 6, Lab 6) | The choreographer gives every planning request its KV blocks from the pool and an arena |
| Threads, mutexes, condition variables, semaphores, barriers | Performers in lockstep, task and request queues, thread pool |
| Deadlock, livelock, starvation | Two performers swapping places, full queues, retry storms |
| Shared memory, `mmap` (Week 12) | The shared stage, the broadcast booth as a separate process |
| Sockets (Week 13) | Every link in the final phase |
| Signals (Week 14) | Graceful shutdown, dead-captain detection |

---

## 2. The Big Picture

```text
                     ┌───────────────────────────────────────┐
                     │      ORCHESTRATOR CENTER (Director)    │
                     │  score · target image · beat clock     │
                     │  choreographer: request queue →        │
                     │  planner pool (KV arena per request)   │
                     └───────┬───────────────────┬───────────┘
        THEME, BEAT, PLAN    │                   │ stage state
                             ▼                   ▼
                ┌─────────────────┐        ┌──────────┐
                │  LEADER drone   │  ...   │ BROADCAST│  (separate GUI,
                │  (captain, zone)│        │ BOOTH    │   read-only)
                └───┬───────┬─────┘        └──────────┘
        keyframes   │       │
              ┌─────▼──┐ ┌──▼─────┐
              │performer│ │performer│ ...  (cheap light points)
              └────────┘ └────────┘
```

### The stage, the clock, and the score

- The sky is a top-down grid (64 × 36 cells in the given scores). Every drone occupies one cell and
  shows one color (0-15). **Each drone is a pixel**; in the finale the swarm is the picture.
- Time is counted in **beats**. A **score** file (given) lists the sections:

```text
stage 64 36
tempo 120                                # beats per minute (tests run at tempo 0)
image data/images/wave64x36.pgm          # the target picture of the finale
section intro   0  15 dawn   calm        # section NAME START_BEAT END_BEAT THEME MOOD
section march  16  31 waves  energetic
section finale 32  55 mosaic triumphant
```

  The theme `mosaic` means "form the target image" (each zone forms its part of it); it is
  always the **last** section. The other themes (`dawn`, `waves`, `hold`, ...) are movement
  formations. Every formation starts and ends in the **home formation** of its zone, so
  sections chain.
- Between beats a performer may move **at most 2 cells**. No two drones may be in the same
  cell on the same beat. Both rules are checked by a given validator; a captain must not
  send a plan that breaks them.
- The beat clock runs at the tempo in the score header. Tests run at "tempo 0" (as fast as
  the system can go), so the logic must be correct, not just timed right.
- **Batteries:** every captain has a battery budget in beats (`--battery`). When it reaches
  zero the captain goes dark, in the middle of the show.

### One protocol, three transports

The same messages travel over *pipes* in Phase 1, *pipes + shared memory* in Phase 2, and
*TCP sockets* in Phase 3. If you write your send/receive code against a file descriptor
and keep protocol parsing separate from the transport, Phase 3 is a small change, not a
rewrite.

| | Phase 1: rehearsal | Phase 2: dress rehearsal | Phase 3: show night |
|---|---|---|---|
| Center | parent process | parent process | server process |
| Leader | child process (`fork`/`exec`) | child process | client process (own socket) |
| Follower | **thread** inside its captain | thread inside its captain | **separate process**, socket to its captain |
| Viewer | the stage drawn as ASCII in the center's terminal (`--ascii`) | **separate process**, reads shared memory (SDL, GTK, or Qt) | separate process, socket client |
| Center ↔ leader | pipes | pipes + shared stage | TCP |
| A captain dies... | its threads die with it; the center restarts the captain (its performers start again at home) | same, and the dead captain's cells in the shared stage must be released | **performers survive as orphans** and must be rehomed |

In Phase 3 *everything communicates through sockets* — center↔leader, leader↔performer,
and viewer↔center. Performers become processes in Phase 3 precisely so that a captain's
death leaves a real orphaned troupe behind (Week 4: when a parent dies, children keep
running).

---

## 3. The Protocol (Frozen)

One message per line, fields separated by single spaces, `\n` terminated, maximum 256
bytes per line. Unknown messages are ignored and logged, never fatal. A *malformed* known
message (wrong field count, not a number, out of range) is rejected, logged, and never crashes
the receiver; the given `protocol.c` already does this for you.

### Center → Leader

```text
ASSIGN <leader_id> <x0> <y0> <x1> <y1> <n_followers>   your zone (inclusive corners)
THEME <section> <start_beat> <end_beat> <theme> <mood> coarse direction for a section
PLAN <req_id> <n>                                      followed by n keyframe lines
BUSY <req_id> <retry_ms>                               the choreographer's queue is full
ADOPT <follower_id>                                    take over an orphaned performer (Phase 3)
BEAT <n>                                               the clock: "start beat n" (all phases)
SHUTDOWN
```

### Leader → Center

```text
HELLO <leader_id> <n_followers>
PLAN_REQ <req_id> <section> <n_followers> <style_seed>  commission detailed choreography
READY <leader_id> <section>                             positions for this section are loaded
POS <leader_id> <beat> <follower> <x> <y> <color>       where a performer is after this beat
ACK <leader_id> <beat>                                  all POS lines of this beat were sent
HEARTBEAT <leader_id>                                   (Phase 3)
DONE <leader_id> <section>
```

`POS` and `ACK` let the center see the stage and **audit** it (collisions, speed) without
trusting the captains; the center never sends `BEAT n+1` before it has the `ACK` for beat `n`.

### Keyframe line (inside `PLAN`, and also what a captain stores)

```text
<beat> <follower_idx> <x> <y> <color>
```

"At `beat`, performer `follower_idx` is at `(x, y)` showing `color`."

A plan is **dense**: it has exactly one keyframe for every beat of the section and every
performer (`n_followers × (end − start + 1)` lines), in beat order. The validator checks this.

### Leader ↔ Follower (Phase 3 only; in Phases 1-2 these are function calls and a barrier)

```text
NEXT <follower_id> <beat>                performer asks for its target for this beat
GOTO <x> <y> <color>                     captain's answer
AT <follower_id> <beat> <x> <y>          performer reports where it is
```

### Orphan rescue (Phase 3)

```text
ORPHANED <follower_id> <old_leader_id> <last_beat>     performer → center
REHOME <follower_id> <leader_host> <leader_port>       center → performer
```

### Center → Broadcast Booth (Phase 3)

```text
FRAME <beat> <w> <h>                     followed by h lines of w color digits
DRONE <id> <x> <y> <role>                one line per drone, after the frame
```

The booth only needs to *eventually* show the latest state. If it is slow, it misses
frames. It must never block the center.

---

## 4. Phases

### Phase 1 — Rehearsal, One Control Booth

**After:** Week 7-8 lectures (threads, synchronization)
**Details:** [`labs/swarm-phase1-threads-sync.md`](../labs/swarm-phase1-threads-sync.md)

The center forks one captain per zone over pipes (your Lab 4 code, reused). Performers
are threads inside their captain, moving in lockstep one beat at a time. The center's
choreographer is a thread pool behind a bounded request queue, with Lab 6 KV blocks and an
arena per request. Each captain keeps an **occupancy grid** for its zone and its performers
must claim a cell before they move into it; the center audits the whole stage from the
captains' `POS` reports. Your job: get a hundred dancers moving without two colliding or a
whole troupe freezing (deadlock), and make the center notice and restart a dead captain.
The skeleton gives you the protocol, planner, validator, clock loop and tests; you build the
queue, the planner pipeline, the grid, the performers and the restart.

### Phase 2 — Dress Rehearsal, a Shared Stage

**After:** Week 12 (virtual memory and IPC)
**Details:** `labs/swarm-phase2-ipc-shared-stage.md`

The stage moves into `mmap`ed shared memory so captains place dancers directly and the
center stops being a bottleneck. The broadcast booth becomes a separate read-only process.
Two dancers must never take the same spot at the same instant — and a captain that dies
holding the stage lock must not freeze the show.

### Phase 3 — Show Night, Everything Wireless

**After:** Week 13-14 (sockets, signals)
**Details:** `labs/swarm-phase3-sockets-signals.md`

Every link is a socket. Performers are separate processes. Drones die — dead batteries,
lost signal. The swarm must notice a silent captain, rehome its orphaned troupe, and
finish the picture. A captain that cannot reach the choreographer keeps dancing in a
simple **local fallback routine** (no protocol change). The final run is one full song:
start the whole swarm with one command and form the target image before the last note.

---

## 5. The Choreographer Interface (the "LLM")

Captains do their thinking through the center's choreographer. It calls one function, so
a real model can be plugged in as a bonus:

```c
/* planner.h — given. Do not change the signature. */
typedef struct {
    int beat;
    int follower;       /* index within the leader */
    int x, y;
    unsigned char color;
} Keyframe;

/* Fill out[] with the keyframes that make n_followers drones perform
 * `theme` in `mood` inside `zone` from start_beat to end_beat: one per beat per drone
 * (dense plan, beat-major). For theme "mosaic" the drones move onto the pixels of the target
 * image inside the zone. style_seed makes different captains produce different (but valid)
 * variations. Returns the number of keyframes written, or -1.
 * The default implementation sleeps for a simulated "thinking time" that grows with the
 * zone and section length, then returns a formation sequence chosen by theme and mood. */
int plan_section(const Theme *theme, const Zone *zone, const Image *target,
                 int n_followers, unsigned style_seed, Keyframe *out, int max);
```

The simulated latency is what makes queueing, thread pools, `BUSY` responses and timeouts
matter. **Captains must check every plan with the given validator** (speed, collisions,
zone) and ask again — or repair it — if it fails: a real model would sometimes get it
wrong. Bonus: replace the stub with an HTTP call to a local or remote model behind the same
function, or write a plan by hand and feed it through the protocol.

---

## 6. Constraints

- C only (the booth may use SDL2, GTK, or Qt).
- Follow the [secure C rules](../labs/secure-c-programming-rules-for-assignments.md).
- No busy-waiting. Use condition variables, semaphores, barriers, or blocking I/O.
- Valgrind (and ThreadSanitizer for Phases 1-2) must be clean on the headless test mode
  (Phase 1: `make test`, `make tsan`; later phases add `--no-gui`). Every program must run
  without a display.
- Every protocol parser must reject malformed and over-long lines without crashing.

---

## 7. Grading

| Phase | Weight |
|---|---|
| Phase 1: threads, queues, barrier, occupancy grid, captain restart, report | 30% |
| Phase 2: shared memory, independent booth, collision test, dead-captain test | 30% |
| Phase 3: sockets, signals, orphan rescue, fallback routine, load test | 30% |
| Final demo and report | 10% |
| Bonus (real model behind the choreographer, new themes, nicer visuals) | up to +15% |

The **performance score** (fraction of keyframes reached on their beat, the mosaic accuracy
in the finale, and the number of safety violations, which must be zero) is reported in each
phase; correctness of the concurrency and fault handling is what is graded.

---

## 8. What We Provide vs. What You Build

| Provided (per phase, with the skeleton code) | You build |
|---|---|
| Score and image loaders, `planner.h`, the stub choreographer | Transport layer, queues, stage, drones |
| The plan validator (speed, collision, zone) | Captain and performer logic |
| Starter viewer (Phase 2) | Booth changes for each phase |
| Test scores and a headless test harness | Your own tests and the report |

You may reuse your Lab 4 orchestrator, Lab 5 pipes, and Lab 6 pool. Earlier years' drone
simulator (`2025-bil222/projects/emergency-drone-coordination-system`) has a thread-safe
list and an SDL view you may study; it is a different architecture, so do not copy it
wholesale.

---

## 9. Report Requirements

Each phase: a short `REPORT.md` with the architecture sketch, the locking scheme (what each
mutex protects and the lock ordering), one race or deadlock you hit and how you found it,
**what happens when a captain dies and how your system notices**, and the AI assistance you
used.
