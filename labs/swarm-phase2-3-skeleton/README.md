# Swarm Phases 2 and 3 skeleton — one channel API, from shared memory to sockets

BIL 322, Final Project (Autonomous Drone Swarm Choreographers), Phases 2 and 3, in one repository.

| Phase | Handout | Due |
|---|---|---|
| 2: IPC — a shared stage, a control channel, a recorder, your booth | `labs/swarm-phase2-ipc-shared-stage.md` | Thursday 26 November 2026 |
| 3: sockets — TCP and UDP transports, the center as a server, performer processes, orphan rescue | `labs/swarm-phase3-sockets-signals.md` | Thursday 10 December 2026 (checkpoint 3 December: Task 1) |

The project is in `projects/project3-drone-swarm-choreographers.md`. The Phase 2 handout's section
"The Source Tree" explains every file, the threads, and the path of a frame from the center to your
window; the Phase 3 handout's "What Is New in the Source Tree" adds the network. Read them before the
code. Linux only (a lab machine, a VM, or WSL2).

## First: move your Phase 1 code in

| Your Phase 1 code | Goes to |
|---|---|
| `src/queue.c` | `common/queue.c` |
| `src/grid.c` | `leader/grid.c` |
| `thinker_main`, `follower_main` bodies | the `PHASE 1` blocks of `leader/leader.c` |
| `handle_plan_req`, `serve_request`, `on_leader_exit` bodies | the `PHASE 1` blocks of `center/center.c` |
| `src/kvpool.c`, `src/arena.c` (optional) | `center/` |

Then `make && tests/run_tests.sh phase1` must pass. Use `send_msg` (leader) and `leader_send`
(center) to send; do not write to file descriptors directly any more.

## Build and run

```bash
make                       # programs in bin/, tests in tests/ (SDL2 is used when installed)
make test                  # Phase 2 checks: PASS/FAIL per check, logs in tests/logs/
make test3                 # Phase 3 checks
tests/run_tests.sh bus     # one part: phase1 | bus | ctl | rec | signals | valgrind   (Phase 2)
tests/run_tests.sh net     #           sockets | net | radio | faults | vg3            (Phase 3)
tests/test_chan shm        # one transport alone: shm | fifo | file (all: those three) | tcp | udp (net: both)
make tsan                  # the show under ThreadSanitizer, driven by commands
make loadtest              # Phase 3: the load test (tests/loadtest.sh), one table row
make demo                  # the heart show with a terminal viewer (q quits the viewer)
./show.sh                  # records the heart show and draws show.png and show.gif (needs Pillow)
```

```bash
bin/center [--name NAME] [--record FILE] [--paused] [Phase 1 options...]
           [--port P [--remote L]... [--heartbeat-ms MS]]          # Phase 3: TCP (P = 0: any port)
bin/leader --center HOST:PORT --id L                               # Phase 3: a captain on another machine
bin/viewer [--name NAME | --center HOST:PORT | --replay FILE | --demo] [--view term|gui|ascii|ppm|check]
           [--wait SEC] [--delay-ms MS] [--scale PX] [--out FILE] [-- your view's options]
bin/swarmctl [--name NAME | --center HOST:PORT] pause | resume | tempo BPM | kill ID | stop | raw LINE | status | watch
```

The default show name is `swarm-<your uid>`, so two students on one machine do not meet. A viewer,
`swarmctl` and the center must use the same `--name` (Phase 2) or the same `--center` port (Phase 3).
With `--port 0` the center prints the port it got: `[center] listening on tcp port 41234`.

`bin/viewer --demo` needs none of your code: it plays a made-up show inside the viewer, so you can
work on your GUI from day one. Without `--port`, everything runs as in Phase 2.

## What is given and what is yours

| File | Status |
|---|---|
| `common/chan.h`, `chan_impl.h`, `chan.c` | given: the channel API, the dispatcher, the pipe transport, the socket helpers |
| `common/chan_shm.c` | **Phase 2, Task 1**: the stage bus (shared memory, newest frame wins) |
| `viewer/view_gui.c` | **Phase 2, Task 2**: your window (a basic SDL2 one is given; any toolkit) |
| `common/chan_fifo.c` (or `chan_mq.c`) | **Phase 2, Task 3**: the control channel |
| `center/center.c` — `apply_command`, `wait_while_paused` | **Phase 2, Task 3**: the operator's commands |
| `common/chan_file.c` | **Phase 2, Task 4**: the flight recorder and replay |
| `common/chan_tcp.c`, `common/chan_udp.c` | **Phase 3, Task 1**: the `tcp:` and `udp:` transports |
| `center/net.c` | **Phase 3, Task 2**: the center's TCP server (interface: `center/net.h`) |
| `leader/radio.c`, `performer/performer.c`, one call in `follower_main` | **Phase 3, Task 3**: performer processes on the radio (interface: `leader/radio.h`) |
| `leader.c` — `heartbeat_main`; `center.c` — `watchdog_main`, `handle_orphan` | **Phase 3, Task 4**: heartbeats and orphan rescue |
| `common/queue.c`, `leader/grid.c`, the `PHASE 1` blocks | yours, from Phase 1 |
| `common/frame.*`, `frame_text.*`, `recording.h`, `swarm.*`, `protocol.*`, `line_io.*` | given |
| `center/`, `leader/`, `viewer/` (the rest), `ctl/swarmctl.c` | given |
| `tests/`, `tools/`, `data/` | given; add your own tests to `tests/run_tests.sh` |

## Programs and channels

```
 Phase 2 (one machine)
 center ──"shm:NAME"──▶ viewers, swarmctl status     the stage bus: Frames, newest wins
   │    ──"file:PATH"─▶ viewer --replay              the flight recorder
   │    ◀─"fifo:NAME"── swarmctl, viewer keys        commands: PAUSE RESUME TEMPO KILL STOP
   └─ pipes (chan_fd) ─▶ captains                     the Phase 1 protocol, unchanged

 Phase 3 (any machines; the Phase 2 channels stay)
 center "tcp::P" ◀── HELLO ──── captains (connect back)  ◀── UDP "radio" ──▶ performer processes
                 ◀── WATCH ──── viewers, swarmctl status        (NEXT GOTO AT WAIT LAND)
                 ◀── OPERATE ── swarmctl, viewer keys
                 ◀── ORPHANED ─ a performer whose captain died → REHOME or LAND
```

The viewer links only `common/` and `viewer/`: it cannot include the center's or the captains' code.

## Debugging tips

* `ls -l /dev/shm /tmp/*.ctl` shows what your Phase 2 transports created (and what they left behind).
* `cat /proc/<pid>/maps | grep shm`: the viewer's mapping of the frames must be `r--s`.
* `ss -tlnp`, `ss -tnp`, `ss -unap`: who listens, who is connected, the radio sockets.
* `pstree -p <center pid>`: captains and their performers; `ps -o pid,ppid,stat,cmd` shows orphans
  and zombies.
* `strace -f -e trace=network,process bin/center --port 0 --tempo 0` shows what really happens.
* `SWARM_UDP_LOSS=30` makes every UDP channel drop 30% of what it sends.
* A hang: `tests/test_chan` prints `HANG` and the step; the center prints `TIMEOUT` after `--timeout`
  seconds. `gdb -p <pid>`, then `thread apply all bt`.
* Never `sleep()` to make a race disappear, and never poll in a loop: the tests measure CPU time.
