/* line_io.c — newline-framed I/O (given). */
#include "line_io.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

void lr_init(LineReader *lr, int fd) {
    lr->fd = fd;
    lr->len = lr->pos = 0;
}

int lr_read_line(LineReader *lr, char *out, size_t n) {
    size_t o = 0;
    int too_long = 0;
    for (;;) {
        if (lr->pos == lr->len) {
            ssize_t r;
            do {
                r = read(lr->fd, lr->buf, sizeof(lr->buf));
            } while (r < 0 && errno == EINTR);
            if (r < 0) {
                return -1;
            }
            if (r == 0) {
                return (o > 0 || too_long) ? -2 : 0;      /* EOF in the middle of a line */
            }
            lr->len = (size_t)r;
            lr->pos = 0;
        }
        char c = lr->buf[lr->pos++];
        if (c == '\n') {
            out[o] = '\0';
            return too_long ? -2 : 1;
        }
        if (o + 1 < n) {
            out[o++] = c;
        } else {
            too_long = 1;                                   /* keep skipping to the newline */
        }
    }
}

int send_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, buf + off, len - off);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

int send_line(int fd, const char *line) {
    char buf[512];
    size_t n = strlen(line);
    if (n + 1 > sizeof(buf)) {
        return -1;
    }
    memcpy(buf, line, n);
    buf[n] = '\n';
    return send_all(fd, buf, n + 1);            /* one write() for the whole line */
}
