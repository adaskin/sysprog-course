# Final Project: Autonomous Drone Swarm Choreographers

**Course:** BIL 322 System Programming
**Topic:** Processes, threads, synchronization, IPC, shared memory, sockets, signals — all in one system
**Mode:** Pairs (individual allowed; expectations are the same)
**Duration:** 3 phases: Phase 1 (1 week); Phases 2 and 3 assigned together with one skeleton, due Thursday 26 November and Thursday 10 December 2026 (Phase 3 checkpoint Thursday 3 December)
**AI use:** Allowed, but must be disclosed and you must be able to explain all code

> **Status: final.** Phase 1's skeleton is in [`labs/swarm-phase1-skeleton/`](../labs/swarm-phase1-skeleton/);
> its protocol is Section 3. Phases 2 and 3 are a **specification project**, independent of Phase 1:
> one skeleton, [`labs/swarm-phase2-3-skeleton/`](../labs/swarm-phase2-3-skeleton/), gives a channel API (seven
> function declarations), the `Frame` layout, a protocol specification
> ([`PROTOCOL.md`](../labs/swarm-phase2-3-skeleton/PROTOCOL.md)) and tests, and no implementation code. Students design
> and write every program.

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
| fork / exec / wait, orphans (Weeks 4-5) | Phase 1: the center launches its captains. Phases 2-3: nothing launches anything, and a dead captain's troupe is orphaned and must be rescued through the protocol |
| Pipes (Week 5) | Phase 1 links |
| Memory pools (Week 6, Lab 6) | Phase 1: the choreographer gives every planning request its KV blocks from the pool and an arena |
| Threads, mutexes, condition variables, semaphores, barriers | Performers in lockstep, request queues and worker pools, a server with many connections |
| Deadlock, livelock, starvation | Two performers swapping places, full queues, retry storms |
| Unix-domain sockets, shared memory, `mmap`, files (Weeks 9-12) | Phase 2: programs that meet by address, the stage bus to the viewers (separate processes), the flight recorder |
| Sockets (Week 13) | Phase 3: TCP between the center and everyone else, a UDP radio between a captain and its performers |
| Signals (Week 14) | A clean stop on `Ctrl-C` on every machine; `SIGKILL` and `SIGSTOP` as the failures to survive |

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
  cell on the same beat. In Phase 1 both rules are checked by a given validator, and a captain
  must not send a plan that breaks them. In Phases 2-3 the center audits every beat, and the
  given `framecheck` audits the recording again.
- The beat clock runs at the tempo in the score header. Tests run at "tempo 0" (as fast as
  the system can go), so the logic must be correct, not just timed right.
- **Batteries (Phase 1):** every captain has a battery budget in beats (`--battery`). When it reaches
  zero the captain goes dark, in the middle of the show.

### Phase 1, then a design of your own

Phase 1 is a guided rehearsal: the center forks its captains, they talk over pipes with the
protocol of Section 3, and the skeleton gives most of the framework. **Phases 2 and 3 start again
from a contract** and do not depend on Phase 1 (you may port your Phase 1 code, or not). Every
program is started on its own, by a person or a test script, on any machine, and the programs find
each other through the center's address: nothing forks, execs or signals anything else. All
communication goes through **one channel API** of seven functions (`chan_open`, `chan_listen`,
`chan_accept`, `chan_send`, `chan_recv`, `chan_addr`, `chan_close`), and the address picks the
transport: `local:PATH`, `shm:NAME` and `file:PATH` in Phase 2, `tcp:HOST:PORT` and
`udp:HOST:PORT` in Phase 3. The same programs run on one machine in Phase 2 and across machines in
Phase 3; only the addresses change. The skeleton's [`PROTOCOL.md`](../labs/swarm-phase2-3-skeleton/PROTOCOL.md)
fixes the messages, the timing and the command lines, and the tests check every program from the
outside, so any team's captain can fly for any team's center.

| | Phase 1: rehearsal | Phase 2: dress rehearsal | Phase 3: show night |
|---|---|---|---|
| Given | most of the framework: protocol code, planner, validator, clock loop, tests | contracts only: `chan.h`, `swarm.h`, `PROTOCOL.md`, tests | the same skeleton |
| Center | parent process | a program that listens at `local:PATH` | the same program at `tcp::PORT` |
| Captain | child process (`fork`/`exec`) | a program of its own: connects, says `HELLO`, gets a zone; late ones wait as standbys | the same, on any machine, with a UDP radio |
| Drone | a thread inside its captain | inside its captain (threads, a pool: your design) | a **performer** process of its own: `JOIN`s through the center, flies by UDP radio |
| Viewer | ASCII in the center's terminal | a program of its own: frames from the stage bus (`shm:`), a recording (`file:`) or text frames; any toolkit | the same, over TCP from any machine |
| Operator | — | `swarmctl`: `PAUSE`, `RESUME`, `TEMPO`, `KILL`, `STATUS`, `STOP` | the same, over TCP |
| Recording | — | every frame to a file; replays; `framecheck` audits it | unchanged |
| A captain dies... | the center restarts it (its performers start again at home) | its drones hover; a standby captain takes the zone | the same, and its performers, now orphans, ask the center and are rehomed to the new captain |
| A captain hangs... | the show waits | a second of silence: declared dead | the same |
| A drone dies... | — | — | its captain reports it `LOST`; a spare performer takes its slot |

Performers become processes in Phase 3 so that a captain's death leaves a real orphaned troupe
behind: still flying, still asking *"Next?"*, and nobody's child, so only the protocol can rescue
them.

---

## 3. The Phase 1 Protocol (Frozen)

> This is the protocol of Phase 1 (pipes, with the given `protocol.c`). Phases 2 and 3 have their
> own contract, the skeleton's [`PROTOCOL.md`](../labs/swarm-phase2-3-skeleton/PROTOCOL.md): the same ideas (zones,
> themes, plans, `BUSY`, beats, `POS` and `ACK`, heartbeats), simplified for programs that meet by
> address, plus the radio (`NEXT`, `GOTO`, `WAIT`, `LAND`) and the rescue (`JOIN`, `ORPHANED`,
> `REHOME`, `ADOPT`).

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
BEAT <n>                                               the clock: "start beat n"
SHUTDOWN
```

### Leader → Center

```text
HELLO <leader_id> <n_followers>                         the first line
PLAN_REQ <req_id> <section> <n_followers> <style_seed>  commission detailed choreography
READY <leader_id> <section>                             positions for this section are loaded
POS <leader_id> <beat> <follower> <x> <y> <color>       where a performer is after this beat
ACK <leader_id> <beat>                                  all POS lines of this beat were sent
HEARTBEAT <leader_id>                                   I am alive
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

In Phase 1 the performers are threads, so a captain talks to them with function calls and a
barrier, and the center draws the stage itself (`--ascii`).

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

### Phase 2 — Dress Rehearsal, a Shared Stage and a Broadcast Booth

**After:** Week 12 (virtual memory and IPC)
**Details:** [`labs/swarm-phase2-ipc-shared-stage.md`](../labs/swarm-phase2-ipc-shared-stage.md)
**Due:** Thursday 26 November 2026

The show becomes separate programs that nobody starts for you: a center that listens at an
address, captains that connect and ask for a zone (a late one waits as a standby and takes over the
zone of a captain that dies), viewers, and an operator tool. You write all of them, and the channel
API under them, from a contract: the skeleton gives seven function declarations, the `Frame`
layout, `PROTOCOL.md` and tests. Three transports carry it: Unix-domain sockets between the
programs; a **stage bus** in shared memory (one writer, any number of readers, the newest frame
wins, never a torn one); and a **flight recorder** file that a viewer replays and `framecheck`
audits. A slow, frozen or killed viewer never slows the show, and a silent captain is declared dead
and replaced.

### Phase 3 — Show Night, Everything Wireless

**After:** Week 13-14 (sockets, signals)
**Details:** [`labs/swarm-phase3-sockets-signals.md`](../labs/swarm-phase3-sockets-signals.md)
**Due:** Thursday 10 December 2026 (checkpoint Thursday 3 December: `tcp:` and `udp:` pass their tests)

The same programs on a network: `tcp:` and `udp:` behind the same API, and only the addresses
change. Every drone becomes a **performer** process, on a machine of its own, that joins through
the center and flies by UDP radio, asking its captain *"Next?"* and asking again when a datagram is
lost. Drones and captains die (`SIGKILL`, `SIGSTOP`, a lossy network): a dead performer's slot goes
to a spare, a dead captain's zone goes to a standby, and its orphaned performers ask the center
where to go and are rehomed. `Ctrl-C` ends everything, on every machine, within a few seconds. A
load measurement finds where the system runs out of breath. Bonuses include a local fallback
routine, a center that survives its own restart, a web viewer, and a **view API** (the center
answers requests with the coordinates of every drone).

---

## 5. The Choreographer Interface (the "LLM")

Captains do their thinking through the center's choreographer. In Phase 1 it is given, behind one
function, so a real model can be plugged in as a bonus:

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
matter. **In Phase 1, captains must check every plan with the given validator** (speed, collisions,
zone) and ask again — or repair it — if it fails: a real model would sometimes get it
wrong. Bonus: replace the stub with an HTTP call to a local or remote model behind the same
function, or write a plan by hand and feed it through the protocol.

**In Phases 2 and 3 the planner is yours.** `PROTOCOL.md` says what a plan must contain (keyframes
as targets inside the zone; the `mosaic` theme forms the image) and the formations are your choice.
It stays slow on purpose, behind a bounded queue that answers `BUSY` when it is full, and the
captains never trust it: a captain checks every move itself.

---

## 6. Constraints

- C only (the booth may use SDL2, raylib, GTK, or Qt; Qt code may be C++ in its one view file).
- Follow the [secure C rules](../labs/secure-c-programming-rules-for-assignments.md).
- No busy-waiting. Use condition variables, semaphores, barriers, or blocking I/O.
- Valgrind and ThreadSanitizer must be clean on the headless tests (Phase 1: `make test`,
  `make tsan`; Phases 2-3: `make test`, `make check2`, `make check3`). Every program must run
  without a display (the viewer's `--text` and `--check`).
- Phases 2-3: no program starts another one (no `fork`, `exec`, `system`, or a signal to another
  program), and after every run nothing of yours is left in `/dev/shm`, `/tmp` or the process
  table.
- Every protocol parser must reject malformed and over-long lines without crashing.

---

## 7. Grading

| Phase | Weight |
|---|---|
| Phase 1: threads, queues, barrier, occupancy grid, captain restart, report | 30% |
| Phase 2: your channel API (`local:`, `shm:`, `file:`), the center, the captain, the viewer and `swarmctl`, tests, `DESIGN.md` | 30% |
| Phase 3: `tcp:` and `udp:`, the center on the network, the radio and the performer, failures and signals, two machines, load measurement | 30% |
| Final demo and report | 10% |
| Bonus (a view API, a real model behind the planner, a web viewer, nicer visuals; see each handout) | up to +15% |

The **performance numbers** (Phase 1: keyframes reached on their beat and the mosaic accuracy;
Phases 2-3: the center's `RESULT` line and `framecheck`) are reported in each phase, and safety
violations must be zero; correctness of the concurrency and fault handling is what is graded.

---

## 8. What We Provide vs. What You Build

| | Provided | You build |
|---|---|---|
| Phase 1 | Score and image loaders, `planner.h` and the stub choreographer, the plan validator, the protocol code, test scores and a headless test harness | The queues, the planner pipeline, the occupancy grid, the performers, the restart, your tests and the report |
| Phases 2-3 | `include/chan.h` (seven declarations), `include/swarm.h` (limits, timing, `Frame`), `PROTOCOL.md`, a `Makefile`, scores and images, `tests/test_chan.c`, `tests/check.sh`, `tests/framecheck.c` | Everything else: the channel API and its five transports, the center (zones, planner, beat clock, audit, frames, rescue), the captain, the performer, the viewer, `swarmctl`, your tests, `DESIGN.md` and the reports |

You may reuse your Lab 4 orchestrator, Lab 5 pipes and Lab 6 pool in Phase 1, and any of your
Phase 1 code in Phases 2-3. Earlier years' drone
simulator (`2025-bil222/projects/emergency-drone-coordination-system`) has a thread-safe
list and an SDL view you may study; it is a different architecture, so do not copy it
wholesale.

---

## 9. Report Requirements

Each phase: a short `REPORT.md` with the architecture sketch, the locking scheme (what each
mutex protects and the lock ordering), one race or deadlock you hit and how you found it,
**what happens when a captain dies and how your system notices**, and the AI assistance you
used. Phases 2-3 also need a one-page `DESIGN.md`: every process and thread, every lock and what
it protects (and their order), and what each message makes happen.
