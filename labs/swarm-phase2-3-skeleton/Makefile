CC      = gcc
WARN    = -Wall -Wextra -std=gnu11 -pthread
CFLAGS  = $(WARN) -g -O2
TSAN    = $(WARN) -g -O1 -fsanitize=thread
LIBS    = -lm -lrt

# Each program sees only its own folder and common/: the viewer cannot even include the
# center's or the captains' headers. That is how the view stays separate from the model.
COMMON_SRC = common/swarm.c common/protocol.c common/line_io.c common/queue.c common/frame.c \
             common/frame_text.c common/chan.c common/chan_shm.c common/chan_fifo.c common/chan_mq.c \
             common/chan_file.c common/chan_tcp.c common/chan_udp.c
COMMON_H   = $(wildcard common/*.h)

# Use YOUR Lab 6 code if you copied it into center/; otherwise the stand-in.
KV         = $(if $(wildcard center/kvpool.c),center/kvpool.c $(wildcard center/arena.c),center/kvpool_stub.c)
CENTER_SRC = center/center.c center/planner.c center/metrics.c center/net.c $(KV)
LEADER_SRC = leader/leader.c leader/grid.c leader/validator.c leader/radio.c
# Task 2: the GUI toolkit, by its pkg-config name. SDL2 is used when it is installed
# (sudo apt install libsdl2-dev); set GUI_PKG = raylib, gtk4, ... if you switch toolkits.
# Without a toolkit, view_nogui.c stands in and `--view gui` says what to install.
GUI_PKG    ?= $(shell pkg-config --exists sdl2 2>/dev/null && echo sdl2)
GUI_CFLAGS  = $(if $(GUI_PKG),$(shell pkg-config --cflags $(GUI_PKG)))
GUI_LIBS    = $(if $(GUI_PKG),$(shell pkg-config --libs $(GUI_PKG)))
GUI_SRC     = $(if $(GUI_PKG),viewer/view_gui.c,viewer/view_nogui.c)
VIEWER_SRC = viewer/viewer.c viewer/view_term.c viewer/view_ascii.c viewer/view_ppm.c \
             viewer/view_check.c $(GUI_SRC)

# The programs go to bin/ (the source folders have the same names).
PROGRAMS = bin/center bin/leader bin/performer bin/viewer bin/swarmctl bin/swapstress bin/gridbench
TESTS    = tests/test_protocol tests/test_planner tests/test_queue tests/test_chan

all: $(PROGRAMS) $(TESTS)

bin:
	mkdir -p bin

bin/center: $(CENTER_SRC) $(COMMON_SRC) $(COMMON_H) $(wildcard center/*.h) | bin
	$(CC) $(CFLAGS) -Icommon -Icenter -o $@ $(CENTER_SRC) $(COMMON_SRC) $(LIBS)

bin/leader: $(LEADER_SRC) $(COMMON_SRC) $(COMMON_H) $(wildcard leader/*.h) | bin
	$(CC) $(CFLAGS) -Icommon -Ileader -o $@ $(LEADER_SRC) $(COMMON_SRC) $(LIBS)

# Phase 3: a performer is its own program (the captain starts one per follower)
bin/performer: performer/performer.c $(COMMON_SRC) $(COMMON_H) | bin
	$(CC) $(CFLAGS) -Icommon -o $@ performer/performer.c $(COMMON_SRC) $(LIBS)

bin/viewer: $(VIEWER_SRC) $(COMMON_SRC) $(COMMON_H) $(wildcard viewer/*.h) | bin
	$(CC) $(CFLAGS) $(GUI_CFLAGS) -Icommon -Iviewer -o $@ $(VIEWER_SRC) $(COMMON_SRC) $(LIBS) $(GUI_LIBS)

bin/swarmctl: ctl/swarmctl.c $(COMMON_SRC) $(COMMON_H) | bin
	$(CC) $(CFLAGS) -Icommon -o $@ ctl/swarmctl.c $(COMMON_SRC) $(LIBS)

bin/swapstress: tools/swapstress.c leader/grid.c leader/grid.h | bin
	$(CC) $(CFLAGS) -Ileader -o $@ tools/swapstress.c leader/grid.c

bin/gridbench: tools/gridbench.c leader/grid.c leader/grid.h | bin
	$(CC) $(CFLAGS) -Ileader -o $@ tools/gridbench.c leader/grid.c

tests/test_protocol: tests/test_protocol.c common/protocol.c common/line_io.c common/swarm.c
	$(CC) $(CFLAGS) -Icommon -o $@ $^ $(LIBS)

tests/test_planner: tests/test_planner.c common/swarm.c center/planner.c leader/validator.c
	$(CC) $(CFLAGS) -Icommon -Icenter -Ileader -o $@ $^ $(LIBS)

tests/test_queue: tests/test_queue.c common/queue.c
	$(CC) $(CFLAGS) -Icommon -o $@ $^

tests/test_chan: tests/test_chan.c $(COMMON_SRC) $(COMMON_H)
	$(CC) $(CFLAGS) -Icommon -o $@ tests/test_chan.c $(COMMON_SRC) $(LIBS)

# ThreadSanitizer builds
bin/center_tsan: $(CENTER_SRC) $(COMMON_SRC) $(COMMON_H) | bin
	$(CC) $(TSAN) -Icommon -Icenter -o $@ $(CENTER_SRC) $(COMMON_SRC) $(LIBS)

bin/leader_tsan: $(LEADER_SRC) $(COMMON_SRC) $(COMMON_H) | bin
	$(CC) $(TSAN) -Icommon -Ileader -o $@ $(LEADER_SRC) $(COMMON_SRC) $(LIBS)

tests/test_queue_tsan: tests/test_queue.c common/queue.c
	$(CC) $(TSAN) -Icommon -o $@ $^

# `make test` runs the checks in tests/run_tests.sh.
test: all
	./tests/run_tests.sh

# `make test3` runs the Phase 3 checks; `make loadtest` the Phase 3 load test (tests/loadtest.sh).
test3: all
	./tests/run_tests.sh phase3

loadtest: all
	./tests/loadtest.sh

# `make tsan` runs the queue test and a show under ThreadSanitizer (the center runs
# bin/leader_tsan as leader), driven by operator commands: tests/tsan_show.sh.
tsan: bin/center_tsan bin/leader_tsan tests/test_queue_tsan bin/swarmctl bin/viewer
	./tests/test_queue_tsan
	./tests/tsan_show.sh

# `make bench` compares one global lock with per-row locks (Phase 1, Task 3).
bench: bin/gridbench
	@for t in 1 2 4 8; do for m in global row; do bin/gridbench $$m $$t 2; done; done

# `make demo`: the heart show in this terminal (the viewer starts first and waits for the show).
demo: bin/center bin/leader bin/viewer
	bin/center --score data/scores/show.score --followers 46 --name demo-$$USER --record show.rec \
	    >demo.log 2>&1 & bin/viewer --wait 10 --name demo-$$USER; wait

clean:
	rm -rf bin $(TESTS) tests/test_queue_tsan

.PHONY: all test test3 loadtest tsan bench demo clean
