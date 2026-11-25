/* linebuf.h -- line-oriented I/O over a TCP socket. Used by BOTH programs.
 * TCP is a byte stream: one recv() can return half a line or several lines. */
#ifndef LINEBUF_H
#define LINEBUF_H

#include <stddef.h>
#include <sys/types.h>
#include "proto.h"

#define IN_CAP (MAX_LINE * 2)

typedef struct {
    int fd;
    char in[IN_CAP];        /* bytes received but not yet consumed */
    size_t in_len;
} linebuf_t;

/* GIVEN: set fd and empty the buffer. */
void lb_init(linebuf_t *lb, int fd);

/* GIVEN: one recv() into the free part of lb->in. Returns what recv returns.
 * Buffer already full: returns -1 with errno = ENOBUFS. */
ssize_t lb_read(linebuf_t *lb);

/* GIVEN: remove ONE complete line from lb->in, preserving later messages.
 *   1: line copied without "\n" / "\r\n" and NUL-terminated;
 *   0: no complete line yet, read more data;
 *  -1: line too long or the output buffer is too small. */
int lb_next_line(linebuf_t *lb, char *line, size_t cap);

/* TODO 1: write ALL n bytes to fd. Handle partial writes and EINTR. Use
 * MSG_NOSIGNAL (given infrastructure prevents SIGPIPE from killing a server).
 * On nonblocking EAGAIN, wait with poll(POLLOUT) and continue.
 * Returns 0 on success, -1 on error. */
int send_all(int fd, const char *buf, size_t n);

#endif
