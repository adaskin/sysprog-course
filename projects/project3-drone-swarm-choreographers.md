# Final Project: Autonomous Drone Swarm Choreographers

**Course:** BIL 322 System Programming  
**Mode:** Pairs (individual work is allowed; expectations are the same)  
**Main topics:** Processes, threads, synchronization, IPC, shared memory, sockets, and signals  
**AI use:** Allowed with disclosure; you must understand and explain your code

## Introduction

In this project, a group of simulated drones performs a synchronized light show. The stage is a grid, time advances in beats, and each drone occupies one cell and displays one color. During the final `mosaic` section, the drones form the target image from the score file.

The project is divided into three phases. Each phase keeps the same basic idea but changes how the parts communicate:

| Phase | Main topic | Communication |
|---|---|---|
| [Phase 1](../labs/swarm-phase1-threads-sync.md) | Threads and synchronization | Center and captains use pipes; followers are threads inside each captain. |
| [Phase 2](project3-phase2-pipes-shared-memory.md) | Local IPC and a live view | Independent programs use FIFOs; the center publishes live frames through shared memory or FIFO messages. |
| [Phase 3](project3-phase3-tcp-udp.md) | Networking and independent drones | The center uses TCP; each drone is a performer process that communicates with its captain over UDP. |

Phase 1 is guided by its lab skeleton. Phases 2 and 3 are project phases in the same repository and use one shared skeleton. You may reuse Phase 1 code, but it is not required.

## System Components

| Component | What it does |
|---|---|
| **Center** | Reads the score, assigns stage zones, plans choreography, runs the beat clock, checks safety, and publishes the current state. |
| **Captain** | Controls the drones in one zone, checks their moves, and reports their positions. An extra captain can wait as a standby. |
| **Drone / performer** | Occupies a cell, follows movement commands, and displays a color. In Phases 1–2 drones live inside captains; in Phase 3 each is a separate process. |
| **Viewer** | Shows the current stage **in real time**. It is read-only unless optional controls are added. |
| **`swarmctl`** | Sends operator commands such as pause, resume, tempo change, kill, status, and stop. |

The center coordinates the show, but captains must still check every move. A plan is only a suggestion: no drone may leave its zone, move faster than `MAX_SPEED`, or enter an occupied cell.

## Communication Overview

### Phase 1

```text
center ──pipes──> captain processes ──shared memory inside each process──> follower threads
```

The emphasis is the bounded thread-safe queue, worker threads, mutexes, condition variables, barriers, and deadlock avoidance.

### Phase 2

```text
captains and swarmctl <──── FIFO messages ────> center
                                                │
                                                ├── shared memory: live binary frames ──> viewers
                                                ├── FIFO text frames: live drone data ────> viewers
                                                └── file: recording for optional replay
```

Phase 2 does not use sockets. FIFOs carry commands and replies. `shm_open` and `mmap` carry the live stage bus. POSIX message queues may be added as an optional extension.

### Phase 3

```text
captains, performers, viewers, swarmctl <──── TCP ────> center
captains                         <──── UDP radio ────> performers
```

TCP replaces FIFOs for programs on different machines. UDP is used for the performer radio because datagrams may be lost, duplicated, or reordered; the protocol handles retries and stale messages.

## One Channel API for Phases 2 and 3

The shared skeleton provides [`include/chan.h`](project3-phase2-3-skeleton/include/chan.h). Its seven function declarations must not change:

```c
chan_open       chan_listen       chan_accept       chan_send
chan_recv       chan_addr         chan_close
```

The address selects the implementation:

- Phase 2: `fifo:PATH`, `shm:NAME`, and `file:PATH`.
- Phase 3 additions: `tcp:HOST:PORT` and `udp:HOST:PORT`.

Keep transport-specific code inside `chan/`. Applications should use the channel functions for IPC. Moving a program from FIFO to TCP should change its address, not add socket calls to the application.

The exact message formats and timing rules are in [`PROTOCOL.md`](project3-phase2-3-skeleton/PROTOCOL.md).

## Real-Time Viewer

A **live visual viewer is required in Phases 2 and 3**. Playing a recording after the show is not a substitute.

After every beat, the center publishes the current state of every drone, including:

- global drone ID,
- zone and slot within that zone,
- `x` and `y` coordinates,
- color,
- state (`none`, `ok`, or `lost`),
- current beat and show status.

A viewer can receive this state in either form:

1. **Shared memory:** complete binary `Frame` messages on `shm:` when the viewer is on the center's machine.
2. **Text frames:** a `FRAME` header followed by `D` records on a `fifo:` or `tcp:` connection after sending `WATCH`.

The viewer should render updates as they arrive using SDL2, GTK, Qt, or a similar toolkit. It may skip old frames to catch up, but it must never block the center. A real-time terminal display is useful for debugging, but the final demo should include the visual viewer.

Recording through `file:` remains useful for auditing, and replay is optional. GUI controls such as pause, resume, tempo, and kill are also optional; the command-line `swarmctl` provides the basic operator interface.

## Common Safety and Process Rules

For all phases:

- Two drones may never occupy the same cell.
- A drone moves at most `MAX_SPEED` cells per beat and stays inside its assigned zone.
- The center audits positions and records collisions and speed violations.
- Malformed input must not crash a program.
- Shutdown must not leave processes or IPC objects behind.

For Phases 2 and 3:

- Programs are started independently; no program forks, execs, or signals another program.
- A slow or stopped viewer must not block the show.
- A dead captain is replaced by a standby when one is available.
- `SIGINT` and `SIGTERM` must produce a clean shutdown.

## Score Files and Demonstrations

The skeleton includes `short.score`, `long.score`, and `show.score`. Use the short score while developing and the full show for the final demonstration.

Assessment is based primarily on working demonstrations rather than student-written test suites. The phase handouts give concrete run examples and demo scenarios, including live viewing, captain failure, performer replacement, UDP packet loss, and clean shutdown.

## Deliverables and Grading

| Component | Weight |
|---|---:|
| Phase 1: queues, synchronization, occupancy grid, restart, and report | 30% |
| Phase 2: FIFO/shared-memory channels, center, captains, live viewer, and demo | 30% |
| Phase 3: TCP/UDP, independent performers, failure handling, live viewer, and demo | 30% |
| Final report and presentation | 10% |

For Phases 2 and 3, submit a short `DESIGN.md` and `REPORT.md` with each phase. Explain the processes, communication channels, shared data, synchronization, and failure handling. Disclose AI assistance and be prepared to explain any part of the implementation.

## Starting Points

- Phase 1: [`labs/swarm-phase1-skeleton/`](../labs/swarm-phase1-skeleton/)
- Phases 2 and 3: [`projects/project3-phase2-3-skeleton/`](project3-phase2-3-skeleton/)
- Shared channel and application contract: [`PROTOCOL.md`](project3-phase2-3-skeleton/PROTOCOL.md)
- Phase 2 handout: [`project3-phase2-pipes-shared-memory.md`](project3-phase2-pipes-shared-memory.md)
- Phase 3 handout: [`project3-phase3-tcp-udp.md`](project3-phase3-tcp-udp.md)
