/* conn.c -- YOUR FILE (TODOs 1-3). Framing, writing, and per-connection processing.
 *
 * TCP is a byte stream: one recv() can return half a line, one line, or three
 * lines glued together. This file turns that stream into request lines.
 * See lecture 10b ("TCP is a byte stream", "Send everything", "Framing"). */
#include "conn.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>

/* TODO 1: see conn.h. Hints:
 *   - search c->in[0 .. in_len) for '\n' (memchr);
 *   - a line may end with "\r\n": strip the '\r' too;
 *   - remember to NUL-terminate `line` and respect `cap`;
 *   - compact the buffer with memmove after removing the line;
 *   - if there is no '\n' and in_len >= MAX_LINE, return -1 (client sends garbage). */
int conn_next_line(conn_t *c, char *line, size_t cap) {
    (void)c; (void)line; (void)cap;
    /* TODO */
    return 0;
}

/* TODO 2: see conn.h. Hints: loop until everything is sent; send(..., MSG_NOSIGNAL);
 * retry on EINTR; on EAGAIN/EWOULDBLOCK poll() the fd for POLLOUT, then continue. */
int send_all(int fd, const char *buf, size_t n) {
    (void)fd; (void)buf; (void)n;
    /* TODO */
    return -1;
}

/* TODO 3: see conn.h. Hints:
 *   char line[MAX_LINE + 1], out[OUT_MAX]; int close_after;
 *   while ((r = conn_next_line(...)) == 1) { ... }
 * Remember: after a read, the buffer can hold SEVERAL requests (pipelining).
 * For "ERR TOOLONG\n" send exactly strlen() bytes (no NUL byte on the wire). */
int conn_serve(conn_t *c) {
    (void)c;
    /* TODO */
    return 0;
}
