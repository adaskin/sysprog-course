# Project 3 — Phase 3: TCP, UDP, and Independent Performers

**BIL 322 System Programming**  
**Due:** Thursday 10 December 2026  
**Checkpoint:** Thursday 3 December — `tcp:` and `udp:` channels working  
**Skeleton:** [`labs/swarm-phase2-3-skeleton/`](../labs/swarm-phase2-3-skeleton/)  
**Submission:** GitHub Classroom repository, Google Classroom link, and final demo  
**Platform:** Linux, a Linux VM, or WSL2

## Objective

Extend your Phase 2 show so that it runs across machines. Add TCP and UDP to the existing channel API, and move each drone into an independent `performer` process.

The center, captains, performers, viewers, and operator tool are still started separately. They find one another through addresses; no program starts another program.

The parts are related as follows:

```text
captains, performers, viewers, swarmctl <──── TCP ────> center
captains                              <──── UDP ────> performers
center ──── live drone IDs, coordinates, colors, and states ────> viewers
```

Performers use TCP briefly to join or request rescue, then use UDP for movement. The center remains the authoritative source of the state shown by every live viewer.

## Reuse the Phase 2 Channel API

Do **not** change [`include/chan.h`](../labs/swarm-phase2-3-skeleton/include/chan.h), and do not replace it with a new networking interface.

| Address | Mechanism | Typical use |
|---|---|---|
| `fifo:PATH` | Named pipes | Phase 2 local communication; keep it working |
| `shm:NAME` | `shm_open` and `mmap` | Stage bus on the center's machine |
| `file:PATH` | File I/O | Recording and replay |
| `tcp:HOST:PORT` | TCP | Center, captains, performers, viewers, and `swarmctl` across machines |
| `udp:HOST:PORT` | UDP | Radio between a captain and its performers |

Your applications must continue to use only `chan_open`, `chan_listen`, `chan_accept`, `chan_send`, `chan_recv`, `chan_addr`, and `chan_close` for IPC. Put the new socket code in `chan/`.

Moving a Phase 2 center, captain, viewer, or `swarmctl` from `fifo:` to `tcp:` should require only a new address, not socket code in the application. TCP has the same line-oriented behavior as `fifo:`. UDP preserves whole datagrams but may lose, duplicate, or reorder them.

The exact behavior is specified in `PROTOCOL.md` Sections 1 and 3.

## What Changes from Phase 2

| Component | Phase 2 | Phase 3 |
|---|---|---|
| Center address | `fifo:PATH` | `tcp:HOST:PORT` |
| Captain | Drones live inside the captain | Captain controls performers through a UDP radio |
| Drone | Thread or other internal representation | Independent `performer` process |
| Viewer | Live visual display from `shm:` or `fifo:` | Also live visual display from `tcp:`; replay remains optional |
| Captain failure | Standby takes over the zone | Standby takes over, and orphaned performers are reassigned |
| Drone failure | Not handled separately | Captain reports `LOST`; a spare performer takes the slot |

Phase 2 transports and behavior must remain available. A viewer on the center's machine may still use live `shm:` frames. `file:` replay is optional and does not replace the real-time viewer.

## Getting Started: A Small Networked Show

Read `PROTOCOL.md` Sections 1.1, 1.2, 1.5, 3.3, 3.4, and 5. Then implement `tcp:` before building the performer protocol.

The following example uses `data/scores/short.score`. `SWARM_BIND` gives each program a different loopback address on one Linux machine, which simulates separate machines.

```bash
# Terminal 1: center
SWARM_BIND=127.0.0.2 bin/center --listen tcp::9000 \
    --score data/scores/short.score --captains 2 --followers 4 \
    --record file:/tmp/$USER.rec

# Terminals 2 and 3: two radio captains
SWARM_BIND=127.0.0.11 bin/captain --center tcp:127.0.0.2:9000 --radio udp::0
SWARM_BIND=127.0.0.12 bin/captain --center tcp:127.0.0.2:9000 --radio udp::0

# Terminal 4: eight performers
for i in $(seq 1 8); do
    SWARM_BIND=127.0.1.$i bin/performer --center tcp:127.0.0.2:9000 &
done

# Terminal 5: viewer and operator
SWARM_BIND=127.0.0.50 bin/viewer --from tcp:127.0.0.2:9000 --gui
SWARM_BIND=127.0.0.51 bin/swarmctl --center tcp:127.0.0.2:9000 status
```

On separate machines, omit `SWARM_BIND` and replace `127.0.0.2` with the center machine's address.

The provided `make test3` and `make check3` commands are optional debugging aids. Your demonstrations are the main assessment.

## Tasks

### Task 1 — Add TCP and UDP to the Channel API (20%)

Implement both transports without changing the API or breaking Phase 2.

- **`tcp:`**: Use the same line rules as `fifo:`. Support host names with `getaddrinfo`, listening on port 0, `chan_addr`, partial reads and writes, timeouts, oversized lines, closed peers, and a bounded connection timeout. `tcp::PORT` listens on all addresses.
- **`udp:`**: One message is one datagram of at most 512 bytes. A client talks to one address. On a server, `chan_send` replies to the sender of the last received datagram.
- **Environment variables:** Support `SWARM_BIND` for both schemes and `SWARM_DROP` for UDP, as described in `PROTOCOL.md`.
- **Closure:** Report closed peers through `CHAN_CLOSED`; never allow `SIGPIPE` to terminate the process.

At the checkpoint, show a TCP captain connected to the center and one performer exchanging `NEXT` and `GOTO` messages over UDP.

### Task 2 — Extend the Center for Performers and TCP (20%)

Keep the Phase 2 center behavior and add the Phase 3 protocol:

- Accept `JOIN` and assign a free performer slot with `REHOME`; notify its captain with `ADOPT`.
- Keep performers without a slot as spares. Give a spare a slot after `LOST`, or send `LAND -1` at shutdown.
- Accept `ORPHANED` and rehome the performer to the captain currently responsible for its zone. If there is no such captain, answer `LAND`.
- Start the show when all zones and performer slots are filled, or when `--wait` expires according to `PROTOCOL.md` Section 5.
- Send live text frames to `WATCH` viewers over TCP, including every drone's ID, zone, slot, coordinates, color, and state after every beat.
- Prevent idle callers, waiting spares, and slow viewers from blocking the beat loop.

You may use a thread per connection, `poll`, or a mixed design. Explain your choice briefly in `DESIGN.md`.

### Task 3 — Implement the UDP Radio and Performer (30%)

**Captain:**

- Open its UDP radio before sending `HELLO <radio_port>` to the center.
- Accept a performer for a slot only after `ADOPT`, then associate that slot with the performer's address.
- On each beat, calculate safe moves and answer `NEXT` with the newest `GOTO` or with `WAIT`. The captain answers requests; it does not initiate radio messages.
- Report a new `POS` only after the performer confirms the move by requesting the next beat.
- Until a move is confirmed, reserve both its old and new cells so that another drone cannot enter either cell.
- Report `LOST <f>` after 3 seconds without contact from a performer that has a pending move.
- During shutdown, answer `NEXT` with `LAND` for half a second before exiting.

**Performer:**

- Send `JOIN` to the center and wait for `REHOME` or `LAND`.
- Ask its captain `NEXT <f> <b>` and retry every 50 ms while waiting.
- Apply `GOTO`, ignore duplicate or stale messages, and confirm a completed move by requesting the next beat.
- After 1.5 seconds without a captain response, or after `CHAN_CLOSED`, send `ORPHANED` to the center and wait for `REHOME` or `LAND`.
- Exit cleanly on `LAND` or when both the captain and center are gone.

All speed, zone, and collision rules from Phase 2 still apply.

### Task 4 — Handle Failures and Signals (15%)

Demonstrate each of these cases:

| Failure | Expected result |
|---|---|
| Kill a performer | Captain reports `LOST`; a spare takes the slot if available. |
| Kill a captain with a standby | Standby takes the zone; orphaned performers are rehomed. |
| Kill a captain without a standby | Zone remains frozen; its orphaned performers receive `LAND`. |
| Set `SWARM_DROP=20` on captains and performers | Retries tolerate datagram loss and the show still completes safely. |
| Send `SIGINT` or `SIGTERM` to the center | Captains land their performers, spares exit, and resources are released. |
| Interrupt a captain, performer, or viewer | The rest of the show continues. |

After every demo, check that no project processes or IPC objects remain.

### Task 5 — Final Demo and Report (15%)

Prepare two main demos:

1. **Short failure demo:** Use `data/scores/short.score`, 2 captains, 4 performers per captain, and at least one spare. Show a performer failure, a captain takeover, and a clean `Ctrl-C` shutdown.
2. **Full show:** Use `data/scores/show.score` with 4 captains and 46 performers per captain. Add 20% UDP loss, kill one captain with a standby available, and let the mosaic finish in the real-time visual viewer. Replay the recording afterwards only as an optional extra.

For the full show, the center command is:

```bash
SWARM_BIND=127.0.0.2 bin/center --listen tcp::9000 --score data/scores/show.score \
    --captains 4 --followers 46 --record file:/tmp/$USER-show.rec
```

Start four radio captains and 184 performers as in the short example, plus at least one standby captain and one spare performer. Set `SWARM_DROP=20` on the captains and performers for the packet-loss demo.

If two machines are available, run the center on one and at least one captain with its performers on the other. This is recommended but not required.

Submit:

- `DESIGN.md` (about one page): processes, important threads, channels, shared data, and lock order.
- `REPORT.md` (about two pages): explain how TCP reuses the Phase 2 channel abstraction, how UDP retries and confirmations work, and what happens after performer and captain failures.
- Disclose AI assistance and be prepared to explain your code.

Optional extensions worth up to +15%: a fallback formation when the center is unavailable, center restart and reconnection, a web viewer, a second server concurrency model, or a read-only drone query API.

## Submission Checklist

Before submitting, verify that:

- Phase 2 FIFO, shared-memory, and file examples still work.
- The short TCP/UDP show completes with zero collisions and zero speed violations.
- A performer can join, move, be lost, and be replaced by a spare.
- Orphaned performers are rehomed after a captain takeover.
- The show tolerates 20% UDP loss.
- Real-time TCP viewers and operator commands work.
- `Ctrl-C` stops the complete system without leftover processes.
- `DESIGN.md` and `REPORT.md` are included.
