/* linebuf.c -- protocol framing is GIVEN; students implement only send_all(). */
#include "linebuf.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>

void lb_init(linebuf_t *lb, int fd)
{
    memset(lb, 0, sizeof(*lb));
    lb->fd = fd;
}

ssize_t lb_read(linebuf_t *lb)
{
    if (lb->in_len >= sizeof(lb->in)) {
        errno = ENOBUFS;
        return -1;
    }
    /* IMPORTANT: one recv() may return half a message or several messages.
     * Bytes are appended; lb_next_line() preserves anything not consumed. */
    ssize_t n = recv(lb->fd, lb->in + lb->in_len,
                     sizeof(lb->in) - lb->in_len, 0);
    if (n > 0) lb->in_len += (size_t)n;
    return n;
}

/* GIVEN: extract one line and retain all bytes belonging to later messages. */
int lb_next_line(linebuf_t *lb, char *line, size_t cap)
{
    if (cap == 0) {
        errno = ENOBUFS;
        return -1;
    }

    char *newline = memchr(lb->in, '\n', lb->in_len);
    if (!newline) {
        if (lb->in_len >= MAX_LINE) {
            errno = EMSGSIZE;
            return -1;
        }
        return 0;
    }

    size_t raw_len = (size_t)(newline - lb->in) + 1;
    if (raw_len > MAX_LINE) {
        errno = EMSGSIZE;
        return -1;
    }

    size_t line_len = raw_len - 1;
    if (line_len > 0 && lb->in[line_len - 1] == '\r') --line_len;
    if (line_len >= cap) {
        errno = ENOBUFS;
        return -1;
    }

    memcpy(line, lb->in, line_len);
    line[line_len] = '\0';
    memmove(lb->in, lb->in + raw_len, lb->in_len - raw_len);
    lb->in_len -= raw_len;
    return 1;
}

/* TODO 1: write ALL n bytes with send(..., MSG_NOSIGNAL). Handle partial
 * sends, retry EINTR, and on EAGAIN/EWOULDBLOCK poll for POLLOUT. */
int send_all(int fd, const char *buf, size_t n)
{
    (void)fd;
    (void)buf;
    (void)n;
    return -1;
}
