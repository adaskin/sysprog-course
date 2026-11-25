/* linebuf.h -- line-oriented I/O over a TCP socket. Used by BOTH programs:
 * the exam server reads requests with it, the student client reads replies.
 *
 * TCP is a byte stream, not messages: one recv() can return half a line, one
 * line, or three lines glued together. linebuf turns the stream into lines. */
#ifndef LINEBUF_H
#define LINEBUF_H

#include <stddef.h>
#include <sys/types.h>
#include "proto.h"

#define IN_CAP (MAX_LINE * 2)       /* room for one full line plus extra bytes */

typedef struct {
    int    fd;
    char   in[IN_CAP];              /* bytes received but not yet consumed */
    size_t in_len;                  /* number of valid bytes in in[] */
} linebuf_t;

/* GIVEN: set fd, empty buffer. */
void lb_init(linebuf_t *lb, int fd);

/* GIVEN: one recv() into the free part of lb->in. Returns what recv returns:
 *   >0 bytes appended, 0 peer closed, -1 error (errno == EAGAIN on a
 *   non-blocking socket with no data, EINTR if interrupted).
 * Buffer already full: returns -1 with errno = ENOBUFS. */
ssize_t lb_read(linebuf_t *lb);

/* TODO 1 (linebuf.c): FRAMING. Remove ONE complete line from lb->in.
 *   returns  1: a line was copied (without "\n" / "\r\n", NUL-terminated) into
 *               line[cap] and removed from the buffer (remaining bytes moved to
 *               the front, in_len updated);
 *            0: no complete line yet, read more data;
 *           -1: line too long (the buffer holds MAX_LINE bytes and no '\n'). */
int lb_next_line(linebuf_t *lb, char *line, size_t cap);

/* TODO 2 (linebuf.c): write ALL n bytes to fd. Handle partial writes and EINTR;
 * never die from SIGPIPE (MSG_NOSIGNAL). On a non-blocking socket EAGAIN means
 * "buffer full": wait with poll(POLLOUT) and continue.
 * Returns 0 on success, -1 on error (peer gone). */
int send_all(int fd, const char *buf, size_t n);

#endif
