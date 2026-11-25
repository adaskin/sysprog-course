/* student.c -- YOUR FILE (TODOs S1-S2). One student = one TCP client.
 * bin/student HOST PORT ID PIN [THINK_MS]
 * The final stdout line must be: student <id>: score <s>/<n>
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "linebuf.h"
#include "proto.h"

#define MAX_REPLY_LINES (MAX_Q + 2 + MAX_Q)

/* GIVEN: checked integer conversion; rejects junk, overflow, and bad ranges. */
static int parse_int_in_range(const char *text, int min, int max, int *out)
{
    char *end;
    if (!text || !*text) return -1;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value < min || value > max)
        return -1;
    *out = (int)value;
    return 0;
}

/* GIVEN: the protocol's end-of-reply rule. */
static int protocol_reply_finished(const char *line)
{
    return !strcmp(line, "END") || !strcmp(line, "OK") ||
           !strcmp(line, "BYE") || !strncmp(line, "OK ", 3) ||
           !strncmp(line, "ERR ", 4);
}

/* TODO S1: create an AF_INET/SOCK_STREAM socket, fill sockaddr_in with
 * htons(port) and inet_pton(host), call connect(), and return the fd.
 * Close the fd and return -1 on failure. This is your FIRST client connection. */
static int connect_to_server(const char *host, int port)
{
    (void)host;
    (void)port;
    return -1;
}

/* GIVEN: read ONE complete reply, retaining bytes for later replies.
 * line_buffer_next_line() may report a partial reply; line_buffer_receive()
 * may also append several replies at once. Never equate recv() with one line. */
static int read_protocol_reply(line_buffer_t *buffer,
                               char lines[][MAX_LINE + 1], int max)
{
    int count = 0;
    if (max <= 0) {
        errno = EINVAL;
        return -1;
    }

    for (;;) {
        char line[MAX_LINE + 1];
        int r = line_buffer_next_line(buffer, line, sizeof(line));
        if (r < 0) return -1;
        if (r == 1) {
            if (count >= max) {
                errno = EMSGSIZE;
                return -1;
            }
            size_t len = strlen(line);
            memcpy(lines[count], line, len + 1);
            count++;
            if (protocol_reply_finished(line)) return count;
            continue;
        }

        ssize_t n = line_buffer_receive(buffer);
        if (n > 0) continue;
        if (n < 0 && errno == EINTR) continue;
        return -1;                       /* EOF or a hard I/O error */
    }
}

/* TODO S2: complete one exam over the already-connected socket:
 *   LOGIN -> require an OK reply and parse num_questions;
 *   QUESTIONS -> read Q lines through END;
 *   usleep(think_ms * 1000);
 *   SUBMIT <answers> -> answers may simply be num_questions 'A' letters;
 *   RESULT -> remember the SCORE <s>/<n> line;
 *   QUIT -> require BYE;
 *   print "student <id>: score <s>/<n>" and return 0.
 * Use socket_send_all(), line_buffer_init(), and read_protocol_reply().
 * Check snprintf() results: a request may use at most MAX_LINE bytes including
 * its newline. Use parse_int_in_range() for numeric fields, not atoi(). */
static int run_exam_session(int fd, int id, const char *pin, int think_ms)
{
    (void)fd;
    (void)id;
    (void)pin;
    (void)think_ms;
    (void)read_protocol_reply;
    return -1;
}

int main(int argc, char **argv)                 /* GIVEN */
{
    if (argc < 5 || argc > 6) {
        fprintf(stderr, "usage: %s HOST PORT ID PIN [THINK_MS]\n", argv[0]);
        return 2;
    }

    int port, id, think_ms = 100;
    if (parse_int_in_range(argv[2], 1, 65535, &port) < 0 ||
        parse_int_in_range(argv[3], FIRST_ID,
                           FIRST_ID + MAX_STUDENTS - 1, &id) < 0 ||
        (argc == 6 &&
         parse_int_in_range(argv[5], 0, 60000, &think_ms) < 0)) {
        fprintf(stderr, "invalid PORT, ID, or THINK_MS (0..60000)\n");
        return 2;
    }

    size_t pin_len = strlen(argv[4]);
    if (pin_len == 0 || pin_len >= 16) {
        fprintf(stderr, "PIN must contain 1..15 characters\n");
        return 2;
    }
    for (size_t i = 0; i < pin_len; i++) {
        if (isspace((unsigned char)argv[4][i])) {
            fprintf(stderr, "PIN cannot contain whitespace\n");
            return 2;
        }
    }

    int fd = connect_to_server(argv[1], port);
    if (fd < 0) {
        fprintf(stderr, "student %d: cannot connect\n", id);
        return 1;
    }
    int rc = run_exam_session(fd, id, argv[4], think_ms);
    close(fd);
    if (rc < 0) fprintf(stderr, "student %d: FAILED\n", id);
    return rc < 0 ? 1 : 0;
}
