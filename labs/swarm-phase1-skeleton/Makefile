CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -g -O2 -pthread -Iinclude
TSAN    = -fsanitize=thread -g -O1 -pthread -Wall -Wextra -std=gnu11 -Iinclude

# Use YOUR Lab 6 code if you copied it into src/; otherwise the stand-in.
KV      = $(if $(wildcard src/kvpool.c),src/kvpool.c $(wildcard src/arena.c),src/kvpool_stub.c)
COMMON  = src/swarm.c src/protocol.c src/line_io.c src/validator.c src/metrics.c src/planner.c
LIBS    = -lm

all: center leader swapstress gridbench tests/test_protocol tests/test_planner tests/test_queue

center: src/center.c $(COMMON) src/queue.c $(KV)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

leader: src/leader.c $(COMMON) src/queue.c src/grid.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

swapstress: tools/swapstress.c src/grid.c
	$(CC) $(CFLAGS) -o $@ $^

gridbench: tools/gridbench.c src/grid.c
	$(CC) $(CFLAGS) -o $@ $^

tests/test_protocol: tests/test_protocol.c src/protocol.c src/line_io.c src/swarm.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

tests/test_planner: tests/test_planner.c src/swarm.c src/planner.c src/validator.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

tests/test_queue: tests/test_queue.c src/queue.c
	$(CC) $(CFLAGS) -o $@ $^

# ThreadSanitizer builds (Task 6)
center_tsan: src/center.c $(COMMON) src/queue.c $(KV)
	$(CC) $(TSAN) -o $@ $^ $(LIBS)

leader_tsan: src/leader.c $(COMMON) src/queue.c src/grid.c
	$(CC) $(TSAN) -o $@ $^ $(LIBS)

tests/test_queue_tsan: tests/test_queue.c src/queue.c
	$(CC) $(TSAN) -o $@ $^

# `make test` runs the checks in tests/run_tests.sh.
test: all
	./tests/run_tests.sh

# `make tsan` runs the same show under ThreadSanitizer (the center runs ./leader_tsan as leader).
tsan: center_tsan leader_tsan tests/test_queue_tsan
	./tests/test_queue_tsan
	LEADER_BIN=./leader_tsan ./center_tsan --score data/scores/short.score --tempo 0 --timeout 120

# `make bench` compares one global lock with per-row locks (Task 3).
bench: gridbench
	@for t in 1 2 4 8; do for m in global row; do ./gridbench $$m $$t 2; done; done

clean:
	rm -f center leader swapstress gridbench center_tsan leader_tsan tests/test_protocol tests/test_planner \
	      tests/test_queue tests/test_queue_tsan

.PHONY: all test tsan bench clean
