/* linebuf.c -- protocol framing is GIVEN; students implement socket_send_all(). */
#include "linebuf.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>

void line_buffer_init(line_buffer_t *buffer, int fd)
{
    memset(buffer, 0, sizeof(*buffer));
    buffer->fd = fd;
}

ssize_t line_buffer_receive(line_buffer_t *buffer)
{
    if (buffer->in_len >= sizeof(buffer->in)) {
        errno = ENOBUFS;
        return -1;
    }
    /* IMPORTANT: one recv() may return half a message or several messages.
     * Bytes are appended; line_buffer_next_line() preserves unused bytes. */
    ssize_t n = recv(buffer->fd, buffer->in + buffer->in_len,
                     sizeof(buffer->in) - buffer->in_len, 0);
    if (n > 0) buffer->in_len += (size_t)n;
    return n;
}

/* GIVEN: extract one line and retain all bytes belonging to later messages. */
int line_buffer_next_line(line_buffer_t *buffer, char *line, size_t cap)
{
    if (cap == 0) {
        errno = ENOBUFS;
        return -1;
    }

    char *newline = memchr(buffer->in, '\n', buffer->in_len);
    if (!newline) {
        if (buffer->in_len >= MAX_LINE) {
            errno = EMSGSIZE;
            return -1;
        }
        return 0;
    }

    size_t raw_len = (size_t)(newline - buffer->in) + 1;
    if (raw_len > MAX_LINE) {
        errno = EMSGSIZE;
        return -1;
    }

    size_t line_len = raw_len - 1;
    if (line_len > 0 && buffer->in[line_len - 1] == '\r') --line_len;
    if (line_len >= cap) {
        errno = ENOBUFS;
        return -1;
    }

    memcpy(line, buffer->in, line_len);
    line[line_len] = '\0';
    memmove(buffer->in, buffer->in + raw_len, buffer->in_len - raw_len);
    buffer->in_len -= raw_len;
    return 1;
}

/* TODO 1: write ALL n bytes with send(..., MSG_NOSIGNAL). Handle partial
 * sends, retry EINTR, and on EAGAIN/EWOULDBLOCK poll for POLLOUT. */
int socket_send_all(int fd, const char *buf, size_t n)
{
    (void)fd;
    (void)buf;
    (void)n;
    return -1;
}
