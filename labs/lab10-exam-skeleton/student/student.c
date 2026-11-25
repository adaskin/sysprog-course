/* student.c -- YOUR FILE (TODOs S1-S3). One student agent = one client program.
 *
 *   bin/student HOST PORT ID PIN [THINK_MS]
 *
 * The student connects to the exam server, logs in, reads its questions,
 * "thinks" for THINK_MS milliseconds (default 100), submits its answers, asks
 * for the result and corrections, and says goodbye. You will start MANY of
 * these programs at the same time (tools/run_students.sh) against ONE server.
 *
 * Output (stdout): the last line must be exactly
 *     student <id>: score <s>/<n>
 * Exit status: 0 on success, 1 on any error (print the reason to stderr).
 * Protocol: see ../common/proto.h and the handout.
 * Framing and send_all come from ../common/linebuf.c (your TODO 1-2). */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "linebuf.h"
#include "proto.h"

/* TODO S1: connect to HOST:PORT over TCP and return the socket fd, or -1.
 * Hints: socket(AF_INET, SOCK_STREAM, 0); inet_pton() for "127.0.0.1" (or
 * getaddrinfo() if you want host names); connect(); perror() on failure. */
static int connect_to(const char *host, int port) {
    (void)host; (void)port;
    /* TODO */
    return -1;
}

/* TODO S2: read ONE complete reply from the server into lines[] and return the
 * number of lines, or -1 on error / connection closed.
 * A reply is finished when a line is "END" or starts with "OK", "ERR" or "BYE".
 * Loop: lb_next_line(); if it returns 0, lb_read() more bytes (0 = server closed). */
#define MAX_REPLY_LINES (MAX_Q + 2 + MAX_Q)
static int read_reply(linebuf_t *lb, char lines[][MAX_LINE + 1], int max) {
    (void)lb; (void)lines; (void)max;
    /* TODO */
    return -1;
}

/* TODO S3: the whole dialogue. In order:
 *   1. send "LOGIN <id> <pin>\n"; the reply must start with "OK"
 *      ("OK <name> <exam_id> <num_questions>": remember num_questions);
 *   2. send "QUESTIONS\n"; read the reply (num_questions "Q ..." lines + "END");
 *   3. usleep(think_ms * 1000)  -- the student is "thinking";
 *   4. build an answer string of num_questions letters from A-D (any strategy:
 *      all 'A', random, or parse the question lines and be clever);
 *      send "SUBMIT <answers>\n"; the reply must be "OK";
 *   5. send "RESULT\n"; print the "WRONG ..." corrections if you like; the first
 *      line is "SCORE <s>/<n>";
 *   6. send "QUIT\n"; read "BYE";
 *   7. print "student <id>: score <s>/<n>" and return 0.
 * Use send_all() to send and read_reply() to receive. Return -1 on any error. */
static int run_session(int fd, int id, const char *pin, int think_ms) {
    (void)fd; (void)id; (void)pin; (void)think_ms; (void)read_reply;
    /* TODO */
    return -1;
}

int main(int argc, char **argv) {                       /* GIVEN */
    if (argc < 5) {
        fprintf(stderr, "usage: %s HOST PORT ID PIN [THINK_MS]\n", argv[0]);
        return 2;
    }
    int port = atoi(argv[2]), id = atoi(argv[3]);
    int think_ms = argc > 5 ? atoi(argv[5]) : 100;
    int fd = connect_to(argv[1], port);
    if (fd < 0) { fprintf(stderr, "student %d: cannot connect\n", id); return 1; }
    int rc = run_session(fd, id, argv[4], think_ms);
    close(fd);
    if (rc < 0) fprintf(stderr, "student %d: FAILED\n", id);
    return rc < 0 ? 1 : 0;
}
