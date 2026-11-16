# Project 3 — Phase 2: Pipes, Shared Memory, and Files

**BIL 322 System Programming**  
**Due:** Thursday 26 November 2026  
**Skeleton:** [`projects/project3-phase2-3-skeleton/`](project3-phase2-3-skeleton/)  
**Submission:** GitHub Classroom repository and Google Classroom link  
**Platform:** Linux, a Linux VM, or WSL2

## Objective

Build the drone show as a set of independent programs on one machine. You will use named pipes (FIFOs), shared memory, threads, synchronization, and signals.

The center, captains, viewers, and operator tool are started separately. They find one another through addresses; no program starts another program.

This phase does **not** use sockets. Phase 3 will add TCP and UDP behind the same channel API.

The parts are related as follows:

```text
captains and swarmctl <──── FIFO commands and replies ────> center
                                                             │
                                                             ├── shm: live binary frames ──> viewers
                                                             ├── fifo: live text frames ────> viewers
                                                             └── file: recording (replay optional)
```

The center is the only owner of the show clock and published stage state. Captains report positions to the center; viewers receive the resulting state from the center rather than communicating with individual drones.

## Programs to Build

| Program | Responsibility |
|---|---|
| `bin/center` | Assign zones, plan choreography, run the beat clock, check movement, publish frames, and handle failures. |
| `bin/captain` | Control the drones in one zone. A captain may also wait as a standby. |
| `bin/viewer` | Render the current drone positions in real time. A graphical viewer is required for the final demo. |
| `bin/swarmctl` | Send operator commands such as `pause`, `resume`, `tempo`, `kill`, `status`, and `stop`. |

In this phase, drones live inside their captain. You may represent them with threads, a thread pool, or another design explained in `DESIGN.md`.

## One Channel API

All communication between programs must use the seven functions declared in [`include/chan.h`](project3-phase2-3-skeleton/include/chan.h). Do not change this header.

| Address | Mechanism | Use in this phase |
|---|---|---|
| `fifo:PATH` | Named pipes (FIFOs) | Center, captains, viewers, and `swarmctl` |
| `shm:NAME` | `shm_open` and `mmap` | Live stage frames from the center to viewers |
| `file:PATH` | File I/O | Recording and replay |

Phase 3 will add `tcp:HOST:PORT` and `udp:HOST:PORT` to this **same API**.

Keep IPC-specific system calls inside `chan/`. The center, captain, viewer, and `swarmctl` should call only the channel functions for IPC, rather than implementing their own FIFO or shared-memory logic.

You may also use POSIX message queues (`mq_open`, `mq_send`, and `mq_receive`) inside the channel layer or as an optional `mq:` transport. Message queues are an extension, not a replacement for the required `fifo:`, `shm:`, and `file:` transports.

The exact behavior is in `PROTOCOL.md` Section 1. In particular:

- `fifo:` sends and receives complete text lines, even when the underlying reads and writes are partial.
- `shm:` has one writer and many readers. The newest frame wins, the writer never waits for readers, and readers never receive torn frames.
- `file:` preserves every message for replay.
- Every receive operation supports a timeout.

## Required Reading and Rules

Read [`README.md`](project3-phase2-3-skeleton/README.md) and [`PROTOCOL.md`](project3-phase2-3-skeleton/PROTOCOL.md) before coding. `PROTOCOL.md` is the authoritative contract for FIFO setup, messages, timing, command-line arguments, frames, and output.

The following rules apply throughout the phase:

1. No program may fork, exec, or signal another program.
2. A drone moves at most `MAX_SPEED` cells per beat and stays inside its zone.
3. Two drones may never occupy the same cell.
4. Captains must check every move, even when the center supplies the plan.
5. A slow, stopped, or disconnected viewer must not block the show.
6. Malformed messages must not crash a program.
7. `SIGINT` and `SIGTERM` must stop the system cleanly.
8. After a run, no FIFOs, shared-memory objects, message queues, or processes may be left behind.

## Getting Started

```bash
cd projects/project3-phase2-3-skeleton
make
make test2
```

The channel tests will fail until you implement the transports. A practical implementation order is:

1. Implement `fifo:` and pass its channel tests.
2. Build a minimal center and captain that complete a show with drones holding position.
3. Add planning and safe movement.
4. Implement `shm:` with `shm_open`, `ftruncate`, and `mmap`; then implement `file:`.
5. Add the live viewer, `swarmctl`, standby takeover, and signal handling.
6. Run the demos below; use the provided test suite as a debugging aid.

A small manual run looks like this:

```bash
# Terminal 1
bin/center --listen fifo:/tmp/$USER.fifo --score data/scores/short.score \
           --captains 2 --followers 8 --bus shm:$USER-show --record file:/tmp/$USER.rec

# Terminals 2 and 3
bin/captain --center fifo:/tmp/$USER.fifo

# Terminal 4: start the real-time GUI viewer while the show is running
bin/viewer --from shm:$USER-show --gui

# Terminal 5, while the show is running
bin/swarmctl --center fifo:/tmp/$USER.fifo pause
bin/swarmctl --center fifo:/tmp/$USER.fifo resume
```

## Tasks

### Task 1 — Implement the Phase 2 Channel Transports (25%)

Implement `fifo:`, `shm:`, and `file:` so that `make test2` passes.

- **`fifo:`**: Implement the connection setup in `PROTOCOL.md` Section 1.1. Support multiple clients, line framing, short reads and writes, timeouts, oversized lines, closed peers, and stale FIFOs. A closed peer must not terminate the process with `SIGPIPE`.
- **`shm:`**: Use a named shared-memory object and memory mapping. Support at least 16 receivers. A slow receiver may skip frames, but it must always receive a complete newer frame. Closing the sender removes the shared-memory object after the last frame remains available to receivers.
- **`file:`**: Store each message as a 4-byte little-endian length followed by its bytes. A truncated final message is treated as the end of the recording.

Follow `PROTOCOL.md` Sections 1.1 and 1.3–1.5 for the exact behavior.

### Task 2 — Implement the Center (30%)

The center must:

- Open its listening FIFO, stage bus, and recording before the show starts.
- Identify each client from its first line: `HELLO`, `WATCH`, or `OPERATE`.
- Assign free zones to captains and keep extra captains as standbys.
- Serve `PLAN_REQ` through a bounded work queue and return `BUSY` when the queue is full.
- Run the beat loop, collect `POS` and `ACK`, and enforce the beat timeout.
- Audit collisions and speed violations after every beat.
- After every beat, publish each drone's ID, zone, slot, coordinates, color, and state to shared memory and connected live text viewers; append the frame to the recording.
- Detect a closed or silent captain. Mark its drones as lost and give its zone to a standby when one is available.
- Handle all operator commands in `PROTOCOL.md` Section 3.5.
- Finish the show with a final frame, shut down clients, remove resources, and print the required `RESULT` line.

Use `PROTOCOL.md` Sections 3–7 for the detailed contract.

### Task 3 — Implement the Captain (20%)

The captain must:

- Connect to the center and send `HELLO 0`.
- Accept a zone assignment or wait as a standby.
- Request plans and retry after `BUSY`.
- Move drones toward their keyframes while enforcing zone, speed, and collision rules.
- Send one `POS` per flying drone and finish each beat with `ACK`.
- Send `HEARTBEAT` every 200 ms, including while waiting or planning.
- Exit immediately on `CRASH` and cleanly on `SHUTDOWN` or a closed center connection.

You may reuse your Phase 1 queue, occupancy grid, or synchronization code, but this is not required.

### Task 4 — Implement the Real-Time Viewer and `swarmctl` (10%)

- `bin/viewer --from ADDR` must display **live** frames from `shm:` or `fifo:` as they arrive.
- Render every drone using its published ID, zone, slot, coordinates, color, and state. Use SDL2, GTK, Qt, or a similar toolkit for the final visual demo.
- The viewer must not wait for the end of the show and must not block the center. It may skip old frames to catch up.
- `--text` and `--check` are useful for debugging, and `file:` replay may be added, but neither replaces the live visual viewer.
- `bin/swarmctl --center ADDR <command>` must send one operator command, print the reply, and return the exit status specified in `PROTOCOL.md` Section 7.
- GUI controls for pause, resume, tempo, and kill are optional. They should send the same operator commands as `swarmctl`.

### Task 5 — Demo, Design, and Report (15%)

Your presentation is the main assessment. The provided `make test2` and `make check2` commands are optional debugging aids; you do not need to write additional tests.

Prepare the following demos:

1. **Short show:** Run `data/scores/short.score` with 2 captains and 8 followers per captain. Show the real-time visual viewer and a final `RESULT` line with zero collisions and zero speed violations.
2. **Full show:** Run `data/scores/show.score` with 4 captains and 46 followers per captain. Show live drone IDs and positions with `shm:` for one viewer and `fifo:` for another.
3. **Captain failure:** Start one extra captain as a standby, kill a captain with `swarmctl`, and show the standby taking over.
4. **Control and shutdown:** Demonstrate `pause`, `resume`, and `tempo`. Stop the center with `Ctrl-C` and show that the other programs exit. You may also demonstrate `viewer --from file:...` as an optional replay.

For the full show, start the center with:

```bash
bin/center --listen fifo:/tmp/$USER-show.fifo --score data/scores/show.score \
           --captains 4 --followers 46 --bus shm:$USER-live --record file:/tmp/$USER-show.rec
```

Then start four captains with `--center fifo:/tmp/$USER-show.fifo` and the live viewer with `--from shm:$USER-live --gui`.

Submit:

- `DESIGN.md` (about one page): processes, threads, IPC objects, shared data, locks, and lock order.
- `REPORT.md` (about two pages): explain your FIFO connection setup, shared-memory synchronization, one race or deadlock you fixed, and how Phase 3 will reuse the same channel API.
- Disclose AI assistance and be prepared to explain your code.

Optional extensions worth up to +10%: a POSIX `mq:` transport, a per-captain viewer, a richer GUI, or replay seeking.

## Submission Checklist

Before submitting, verify that:

- The short and full shows complete with zero collisions and zero speed violations.
- Real-time shared-memory and FIFO viewing work; file replay is optional.
- A standby takes over after a captain dies.
- Operator commands and `Ctrl-C` shutdown work.
- No processes, FIFOs, shared-memory objects, or message queues remain after a run.
- `DESIGN.md` and `REPORT.md` are included.

In Phase 3, keep this API and these transports working. You will add TCP and UDP backends and replace each drone inside a captain with an independent performer process.
