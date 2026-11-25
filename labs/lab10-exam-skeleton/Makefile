CC      = gcc
CFLAGS  = -std=gnu11 -O2 -g -Wall -Wextra -Wpedantic -pthread -Icommon -Iexam-server
LDLIBS  = -pthread

COMMON  = common/linebuf.c
SERVER  = exam-server/main.c exam-server/exam.c exam-server/conn.c \
          exam-server/srv_iterative.c exam-server/srv_thread.c exam-server/srv_fork.c \
          exam-server/srv_select.c exam-server/srv_poll.c exam-server/srv_epoll.c
STUDENT = student/student.c
MODES   = iterative thread fork select poll epoll

all: bin/examsrv bin/student

bin/examsrv: $(SERVER) $(COMMON) $(wildcard exam-server/*.h common/*.h)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SERVER) $(COMMON) $(LDLIBS)

bin/student: $(STUDENT) $(COMMON) $(wildcard common/*.h)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(STUDENT) $(COMMON) $(LDLIBS)

# deterministic mock data: 1000 students, 3 exams x 20 questions (same for everyone)
data: data/students.csv
data/students.csv: tools/gen_data.py
	python3 tools/gen_data.py --seed 322 --students 1000 --exams 3 --questions 20 --out data

# make test MODE=epoll [N=300]  : start the server, check it, run N student programs, stop it
test: all data
	sh tools/test_server.sh $(MODE) $(N)

# every model (iterative uses fewer students; select and fork are homework)
test-all: all data
	@for m in $(MODES); do sh tools/test_server.sh $$m $(N); done

reset:
	rm -rf results && mkdir -p results

clean:
	rm -rf bin results

.PHONY: all data test test-all reset clean
