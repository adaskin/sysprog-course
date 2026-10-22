# Swarm Phase 2: Shared Stage and an Independent Broadcast Booth

**BIL 322 System Programming — Final Project, Phase 2**
**Duration:** 1 week
**Project:** [Autonomous Drone Swarm Choreographers](../projects/project3-drone-swarm-choreographers.md)
**Submission:** GitHub Classroom (repository) + Google Classroom link

> **Status: DRAFT.** Details, numbers, and the skeleton code are finalized the
> week this phase is assigned.

---

## 🎯 Context

*Dress rehearsal, a shared stage.*

In Phase 1 the center owned the stage and every move was reported to it. That makes the
center a bottleneck, and it ties the GUI to the center's process.

Now the **stage state** (current beat, every drone's cell and color) moves into
**shared memory**. Leaders update their own drones directly. The viewer becomes a
**separate process** that maps the same memory read-only and redraws whenever it changes.
Themes, planning requests, and plans still go over pipes, as in Phase 1.

## 📚 Learning Objectives

1. Create and share a region of memory with `mmap` (`MAP_SHARED`, file-backed or `shm_open`).
2. Use **process-shared** synchronization (`PTHREAD_PROCESS_SHARED` mutexes or POSIX named
   semaphores) and explain why a normal mutex in shared memory is not enough.
3. Design a **lock-free-for-readers** update scheme with a frame counter (seqlock idea).
4. Decouple a GUI from the simulation so a slow viewer cannot slow the show.
5. Detect and prevent two drones taking the same cell.

## 🧰 Prerequisites

- Phase 1 complete (your queue, protocol code, tests).
- Week 12 lecture (virtual memory, `mmap`, shared memory, FIFOs).

---

## 🏗️ Architecture for This Phase

```text
center ──pipes (THEME, PLAN_REQ, PLAN, ...)──▶ leaders (fork/exec, as in Phase 1)
   │                                             │
   │                                             └─ follower threads update their own
   ▼                                                drones in the shared stage ──┐
 creates shared memory ──────────────────────▶  [ header | drones | occupancy ] ◀┘
                                                           ▲
 viewer (separate process) ─ maps read-only ───────────────┘
```

Shared layout (given as `shared.h`; you may add fields but not reorder the header):

```c
typedef struct {
    pthread_mutex_t  write_lock;    /* process-shared, robust; protects writers  */
    volatile unsigned long frame;   /* incremented after each beat is applied    */
    volatile int  beat;
    int  width, height;
    int  n_drones;
    /* followed in memory by: DroneState drones[n_drones]; unsigned short occ[w*h]; */
} SharedHeader;
```

---

## 🛠️ Task 1: Create and Map the Shared Stage (25%)

- The center creates the shared memory (name from the command line), initializes the header
  (process-shared, robust mutex attributes), and passes the name to the leaders and viewer.
- Leaders map it read-write. The viewer maps it **read-only** (`PROT_READ`); an attempted
  write by the viewer must crash with `SIGSEGV` in your test.
- Cleanup: the center unlinks the shared memory on every exit path.

## 🔒 Task 2: Moving Without a Funnel (30%)

- A follower moves with one function, `stage_move(id, x, y, color)`, which takes the lock,
  checks that the destination cell is free, updates the drone and occupancy, and bumps `frame`
  at the end of the beat.
- **Collision rule:** two drones must never be in the same cell on the same beat. Decide the
  mechanism (per-cell claim, per-zone ownership, or the lock alone) and **justify it in the
  report**. Zone ownership by leader is allowed, but the finale lets zones overlap (the given
  score has a `converge` section), so you need an explicit rule at the boundaries.
- What happens if a leader dies **while holding** `write_lock`? Use a *robust* mutex
  (`PTHREAD_MUTEX_ROBUST`) and handle `EOWNERDEAD`. Write a test that kills a leader at the
  worst time.

## 🖥️ Task 3: The Independent Viewer (25%)

- Separate executable: `viewer <shm-name>`. SDL2, GTK, or Qt — your choice; keep the drawing
  code in one file.
- The viewer **never takes the writer lock**. It reads `frame`, copies the stage, and re-reads
  `frame`; if it changed during the copy, it copies again (or accepts a torn frame and says so
  in the report). Explain why this is safe for display and unsafe for anything else.
- Slow-viewer test: add `--delay-ms 500` to the viewer; the show's completion time must not change.
- `--no-gui` mode that writes the last frame to a PPM file, for the grader.

## 📊 Task 4: Tests and Report (20%)

- Headless run with 4 leaders × 16 followers performs the short score with zero safety violations.
- Collision test: all followers are told to enter the same 4 cells; no cell is ever shared.
- Kill-a-leader test (robust mutex), slow-viewer test. When a captain dies, the center releases that
  captain's cells in the shared stage (and restarts it, as in Phase 1); a test shows no cell stays
  occupied by a dead drone.
- Run under ThreadSanitizer and Valgrind. Use `ls /dev/shm` to prove nothing is leaked after a
  normal exit and after `SIGINT`.
- `REPORT.md`: the shared layout, who writes what, the collision rule, the seqlock-style read,
  the crash behavior, benchmark vs Phase 1, AI assistance.

---

## 🚀 Bonus (up to +10%)

- B1: Double buffering: two stage copies and an atomic swap, so the viewer never sees torn frames.
- B2: A FIFO-based control channel for a command-line tool (`swarmctl pause`, `swarmctl status`).
- B3: A trail effect in the viewer (fade the last few beats), computed in the viewer only.

## ✅ Grader's Checklist

```bash
make && make test
ls /dev/shm | grep swarm            # empty after a normal exit
valgrind --leak-check=full --error-exitcode=1 ./center --no-gui --test
```

Zero-tolerance items: leaked shared memory, a viewer that takes the writer lock, a
non-robust mutex, a hang when a leader dies, a missing report.

## 🔭 Where This Goes Next

Phase 3 removes shared memory and pipes entirely: every link becomes a socket, the viewer
becomes a network client, and `SIGINT`/`SIGPIPE`/leader crashes are handled system-wide.
