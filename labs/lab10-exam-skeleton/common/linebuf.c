/* linebuf.c -- YOUR FILE (TODOs 1-2). Used by the server AND the student.
 * See lecture 10b ("TCP is a byte stream", "Send everything", "Framing"). */
#include "linebuf.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>

void lb_init(linebuf_t *lb, int fd) {          /* GIVEN */
    memset(lb, 0, sizeof(*lb));
    lb->fd = fd;
}

ssize_t lb_read(linebuf_t *lb) {               /* GIVEN */
    if (lb->in_len >= sizeof(lb->in)) { errno = ENOBUFS; return -1; }
    ssize_t n = recv(lb->fd, lb->in + lb->in_len, sizeof(lb->in) - lb->in_len, 0);
    if (n > 0) lb->in_len += (size_t)n;
    return n;
}

/* TODO 1: see linebuf.h. Hints:
 *   - search lb->in[0 .. in_len) for '\n' (memchr);
 *   - a line may end with "\r\n": strip the '\r' too;
 *   - NUL-terminate `line` and respect `cap`;
 *   - compact the buffer with memmove after removing the line;
 *   - no '\n' and in_len >= MAX_LINE -> return -1 (garbage from the peer). */
int lb_next_line(linebuf_t *lb, char *line, size_t cap) {
    (void)lb; (void)line; (void)cap;
    /* TODO */
    return 0;
}

/* TODO 2: see linebuf.h. Hints: loop until everything is sent; send(..., MSG_NOSIGNAL);
 * retry on EINTR; on EAGAIN/EWOULDBLOCK poll() the fd for POLLOUT, then continue. */
int send_all(int fd, const char *buf, size_t n) {
    (void)fd; (void)buf; (void)n;
    /* TODO */
    return -1;
}
