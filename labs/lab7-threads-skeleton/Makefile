CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -g -O2 -pthread
TSAN    = -fsanitize=thread -g -O1 -pthread -Wall -Wextra -std=gnu11
SIZE_MB ?= 64

PROGS = agents agents_bug tokencount creation_bench race race_fixed tok_race gen_data
TSAN_PROGS = tokencount_tsan race_fixed_tsan

all: $(PROGS)

agents agents_bug tokencount creation_bench race race_fixed tok_race swarm: %: %.c common.h
	$(CC) $(CFLAGS) -o $@ $<

gen_data: gen_data.c
	$(CC) $(CFLAGS) -o $@ $<

# Generate the benchmark input (default 64 MB; change with SIZE_MB=200). Do NOT commit it.
data: gen_data
	./gen_data data/big.txt $(SIZE_MB)

tokencount_tsan: tokencount.c common.h
	$(CC) $(TSAN) -o $@ $<

race_fixed_tsan: race_fixed.c common.h
	$(CC) $(TSAN) -o $@ $<

tsan: $(TSAN_PROGS)
	./run_tsan.sh

test: all
	./tests/run_tests.sh

clean:
	rm -f $(PROGS) swarm $(TSAN_PROGS) tests/long_line.txt

.PHONY: all data tsan test clean
