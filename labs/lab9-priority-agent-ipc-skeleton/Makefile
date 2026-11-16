CC      = gcc
CFLAGS  = -std=gnu11 -O2 -g -Wall -Wextra -Wpedantic -pthread
LDLIBS  = -pthread -lrt

SOURCES = main.c runner.c pool.c work.c mutex_queue.c mq_queue.c shm_queue.c
HEADERS = agentlab.h

all: agentlab

agentlab: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

check: agentlab
	@set -e; for backend in mutex mq shm; do \
		./agentlab $$backend 1 128; \
		./agentlab $$backend 4 512; \
	done

compare: agentlab
	sh compare.sh

clean:
	rm -f agentlab

.PHONY: all check compare clean
