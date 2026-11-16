# Swarm Phases 2 and 3: the skeleton

BIL 322 System Programming, final project
([Autonomous Drone Swarm Choreographers](../project3-drone-swarm-choreographers.md)).
Phase 2 is due **Thursday 26 November 2026**, Phase 3 **Thursday 10 December 2026** (checkpoint
Thursday 3 December). Handouts: [Phase 2](../project3-phase2-pipes-shared-memory.md),
[Phase 3](../project3-phase3-tcp-udp.md).

This skeleton gives you **contracts, not code**. You design and write every program and every
transport yourself, in any structure you like. Phases 2 and 3 do not depend on Phase 1: you may port
your Phase 1 code (queues, grid, threads), or start again.

## What is given

| Path | What it is | May you change it? |
|---|---|---|
| [`include/chan.h`](include/chan.h) | The channel API: seven function declarations. You write them. | No |
| [`include/swarm.h`](include/swarm.h) | The limits, the timing constants, and the `Frame` struct (the one binary message). | No |
| [`PROTOCOL.md`](PROTOCOL.md) | The contract: channels, messages, timing, the show, frames, command lines. | No |
| `data/` | Scores and target images. | Add your own |
| `tests/test_chan.c` | Tests of your channel API, scheme by scheme (`make test`, `make test2`, `make test3`). | No |
| `tests/check.sh` | Whole shows, tested from the outside (`make check2`, `make check3`). | No |
| `tests/framecheck.c` | Audits any recording: collisions, speed, torn or missing frames. | No |
| `Makefile` | Builds each program from the `.c` files in its folder. | Yes |

Everything else is yours: the five programs, the channel code, the threads and locks, the planner,
the recorder, the viewer, and how a captain survives when things go wrong.

## Where your code goes

```text
chan/        your channel API: chan_open, chan_listen, chan_accept, chan_send, chan_recv,
             chan_addr, chan_close, and one scheme per file if you like (fifo, shm, file, tcp, udp)
common/      code shared by several programs, if any (message parsing, the frame checksum, ...)
center/      bin/center      the center: zones, planner, beat clock, audit, frames, operators
captain/     bin/captain     a captain: its zone, its drones, its grid, (Phase 3) its radio
performer/   bin/performer   Phase 3: one drone, a process of its own
viewer/      bin/viewer      real-time visual view (shm:, fifo:, or tcp:); file: replay optional
swarmctl/    bin/swarmctl    the operator's command-line tool
```

`make` builds `bin/P` from `P/*.c` + `chan/*.c` + `common/*.c`, with `include/`, `common/` and
`chan/` on the include path. The viewer gets `-DHAVE_SDL2` and the SDL2 libraries when SDL2 is
installed (`sudo apt install libsdl2-dev`). A text viewer is useful for debugging, but the final demo requires a live graphical viewer.

Write a one-page **`DESIGN.md`** at the top of your repository: your processes and threads, every
lock and what it protects (and their order), and what each message makes happen. It is graded.

## The rules that matter most

Read [`PROTOCOL.md`](PROTOCOL.md) in full. In short:

- **Nothing starts anything.** No program forks, execs or signals another one. The center, every
  captain, every performer, every viewer and `swarmctl` are started on their own (by you, by the
  test script, on any machine) and find each other through the center's address.
- **One API, two worlds.** The same programs run on one machine in Phase 2 (`fifo:`, `shm:`,
  `file:`) and across machines in Phase 3 (`tcp:`, `udp:`). Only the addresses change.
- **Optional message queues.** You may use POSIX message queues inside `chan/` or add an optional
  `mq:` scheme. The required Phase 2 schemes remain `fifo:`, `shm:`, and `file:`.
- **Safety first.** No two drones in one cell, no drone faster than 2 cells per beat, ever, whatever
  dies or gets lost. The center audits every beat; `framecheck` audits your recording again.
- **Keep the command lines** of PROTOCOL.md Section 7 and the `RESULT` line: the tests use them. You
  may add options.

## Running a show

Phase 2, one machine, one terminal each (or `&`):

```bash
make
bin/center --listen fifo:/tmp/$USER.fifo --score data/scores/show.score --captains 4 --followers 46 \
           --bus shm:$USER-show --record file:/tmp/$USER.rec
bin/captain --center fifo:/tmp/$USER.fifo          # four times
bin/viewer --from shm:$USER-show --gui              # live view during the show
bin/swarmctl --center fifo:/tmp/$USER.fifo pause   # resume, tempo 300, kill 2, status, stop
```

Phase 3, every program on its own machine (or on one machine, each with its own address):

```bash
SWARM_BIND=127.0.0.2  bin/center --listen tcp::9000 --score data/scores/show.score --captains 4 --followers 46
SWARM_BIND=127.0.0.11 bin/captain --center tcp:127.0.0.2:9000 --radio udp::0          # 127.0.0.12, ...
SWARM_BIND=127.0.1.1  bin/performer --center tcp:127.0.0.2:9000                      # one per drone
SWARM_BIND=127.0.0.50 bin/viewer --from tcp:127.0.0.2:9000 --gui
```

On real machines, leave out `SWARM_BIND` and use the center machine's address.

## Optional Debugging Aids

Assessment is based on the demonstrations in the phase handouts. These commands can help you debug.

```bash
make test2                        # your channel API, Phase 2 schemes (bin/test_chan fifo shm file)
make test3                        # Phase 3 schemes (bin/test_chan tcp udp); make test: all five
make check2                       # Phase 2 shows: tests/check.sh phase2 [scenario ...]
make check3                       # Phase 3 shows: tests/check.sh phase3 [scenario ...]
bin/framecheck /tmp/$USER.rec     # audit one of your recordings
tests/check.sh list               # the scenarios
```

`check.sh` keeps every program's output in `tests/logs/<scenario>/`. When a scenario fails, read
the center's `.err` first. Follow the protocol to the letter: any team's captain should be able to
fly for any team's center. Try it with another team before the deadline.
