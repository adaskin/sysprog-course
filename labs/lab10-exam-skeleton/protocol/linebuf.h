/* linebuf.h -- line-oriented I/O over a TCP socket. Used by BOTH
 * programs. TCP is a byte stream: one recv() can return half a line
 * or several lines. */
#ifndef LINEBUF_H
#define LINEBUF_H

#include <stddef.h>
#include <sys/types.h>

#include "proto.h"

#define LINE_BUFFER_CAP (MAX_LINE * 2)

typedef struct {
    int fd;
    char in[LINE_BUFFER_CAP]; /* bytes received but not yet consumed */
    size_t in_len;
} line_buffer_t;

/* GIVEN: set fd and empty the buffer. */
void line_buffer_init(line_buffer_t* buffer, int fd);

/* GIVEN: one recv() into the free part of buffer->in. Returns recv's
 * result. Buffer already full: returns -1 with errno = ENOBUFS. */
ssize_t line_buffer_receive(line_buffer_t* buffer);

/* GIVEN: remove ONE complete line, preserving partial/later messages.
 *   1: line copied without "\n" / "\r\n" and NUL-terminated;
 *   0: no complete line yet, read more data;
 *  -1: line too long or the output buffer is too small. */
int line_buffer_next_line(line_buffer_t* buffer, char* line,
                          size_t cap);

/* TODO 1: write ALL n bytes with send(..., MSG_NOSIGNAL). Handle
 * partial sends, retry EINTR, and on EAGAIN/EWOULDBLOCK poll for
 * POLLOUT. Returns 0 on success, -1 on error. */
int socket_send_all(int fd, const char* buf, size_t n);

#endif
