CC      = gcc
CFLAGS  = -std=gnu11 -O2 -g -Wall -Wextra -Wpedantic -pthread
LDLIBS  = -pthread

SOURCES = main.c exam.c conn.c srv_thread.c srv_fork.c srv_select.c srv_poll.c srv_epoll.c
HEADERS = exam.h conn.h srv.h
MODES   = thread fork select poll epoll

all: examsrv

examsrv: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

# deterministic mock data: 1000 students, 3 exams x 20 questions (same for everyone)
data: data/students.csv
data/students.csv: tools/gen_data.py
	python3 tools/gen_data.py --seed 322 --students 1000 --exams 3 --questions 20 --out data

# make test MODE=epoll   (checks + load test for one server)
test: examsrv data
	sh tools/test_mode.sh $(MODE)

# every mode; a mode that is still a TODO is reported as FAIL (select and fork are optional)
test-all: examsrv data
	@for m in $(MODES); do sh tools/test_mode.sh $$m; done

reset:
	rm -rf results && mkdir -p results

clean:
	rm -rf examsrv results

.PHONY: all data test test-all reset clean
