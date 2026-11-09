/* line_io.h — newline-framed I/O on a file descriptor (given). Works on pipes now; you will
 * own and extend it for sockets in Phase 3 (short reads and writes, SIGPIPE). */
#ifndef LINE_IO_H
#define LINE_IO_H

#include <stddef.h>

typedef struct {
    int    fd;
    char   buf[4096];
    size_t len, pos;
} LineReader;

void lr_init(LineReader *lr, int fd);

/* Read one line into out (without the newline, NUL-terminated).
 *   1  got a line      0  EOF      -1  read error
 *  -2  the line was longer than n-1 bytes: it was skipped up to its newline */
int lr_read_line(LineReader *lr, char *out, size_t n);

/* Write line + '\n', looping over short writes and EINTR. 0 ok, -1 error (EPIPE: peer is gone).
 * The caller must hold whatever lock protects the descriptor: lines from different threads must
 * not interleave. */
int send_line(int fd, const char *line);

/* Write all of buf (several lines at once). 0 ok, -1 error. */
int send_all(int fd, const char *buf, size_t len);

#endif
