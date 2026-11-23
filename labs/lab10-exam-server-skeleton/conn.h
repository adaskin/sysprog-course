/* conn.h -- per-connection state and the helpers YOU implement in conn.c. */
#ifndef CONN_H
#define CONN_H

#include <stddef.h>
#include <sys/types.h>
#include "exam.h"

#define IN_CAP (MAX_LINE * 2)       /* input buffer: room for one full line + more */

typedef struct {
    int       fd;
    char      in[IN_CAP];           /* bytes received but not yet processed */
    size_t    in_len;               /* number of valid bytes in in[] */
    session_t sess;                 /* login state, owned by the exam service */
} conn_t;

/* GIVEN: zero the struct, set fd, session_init(). */
void conn_init(conn_t *c, int fd);

/* GIVEN: one recv() into the free part of c->in. Returns what recv returns:
 *   >0 bytes appended, 0 peer closed, -1 error (errno == EAGAIN on a
 *   non-blocking socket with no data; EINTR if interrupted).
 * If the buffer is already full it returns -1 with errno = ENOBUFS. */
ssize_t conn_read(conn_t *c);

/* TODO 1 (conn.c): FRAMING. Remove ONE complete request line from c->in.
 *   returns  1: a line was copied (without "\n" / "\r\n", NUL-terminated) into
 *               line[cap] and removed from the buffer (remaining bytes moved to
 *               the front, in_len updated);
 *            0: no complete line yet, need more data;
 *           -1: line too long (buffer holds MAX_LINE bytes and no '\n'). */
int conn_next_line(conn_t *c, char *line, size_t cap);

/* TODO 2 (conn.c): write ALL n bytes to fd. Handle partial writes, EINTR, and
 * never die from SIGPIPE (use MSG_NOSIGNAL). On a non-blocking socket, EAGAIN
 * means "buffer full": wait with poll(POLLOUT) and continue.
 * Returns 0 on success, -1 on error (peer gone). */
int send_all(int fd, const char *buf, size_t n);

/* TODO 3 (conn.c): process EVERY complete line currently in c->in:
 *   loop conn_next_line -> exam_handle_line -> send_all.
 *   too long line  -> send "ERR TOOLONG\n" and return -1
 *   QUIT (close_after) -> send the reply and return -1
 *   send_all fails -> return -1
 * Returns 0 if the connection stays open, -1 if the caller must close it. */
int conn_serve(conn_t *c);

#endif
