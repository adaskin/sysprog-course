CC      = gcc
CFLAGS  = -std=gnu11 -O2 -g -Wall -Wextra -Wpedantic -pthread -Iprotocol -Iexam-server
LDLIBS  = -pthread

PROTOCOL = protocol/linebuf.c
SERVER   = exam-server/server.c exam-server/exam.c
STUDENT  = student/student.c
MODES    = iterative thread fork select poll epoll

all: bin/examsrv bin/student

bin/examsrv: $(SERVER) $(PROTOCOL) $(wildcard exam-server/*.h protocol/*.h)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SERVER) $(PROTOCOL) $(LDLIBS)

bin/student: $(STUDENT) $(PROTOCOL) $(wildcard protocol/*.h)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(STUDENT) $(PROTOCOL) $(LDLIBS)

data: data/students.csv
data/students.csv: tools/gen_data.py
	python3 tools/gen_data.py --seed 322 --students 1000 --exams 3 --questions 20 --out data

test: all data
	sh tools/test_server.sh $(MODE) $(N)

test-all: all data
	@for m in $(MODES); do sh tools/test_server.sh $$m $(N); done

reset:
	rm -rf results && mkdir -p results

clean:
	rm -rf bin results

.PHONY: all data test test-all reset clean
