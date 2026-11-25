/* server.h -- per-connection state of the exam server and the helper YOU
 * implement in conn.c. The line framing itself lives in ../common/linebuf.c. */
#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include "exam.h"
#include "linebuf.h"

typedef struct {
    linebuf_t lb;           /* socket fd + received-bytes buffer (lb.fd is the client socket) */
    session_t sess;         /* login state, owned by the exam service */
} conn_t;

/* GIVEN (main.c): initialise for a freshly accepted socket. */
void conn_init(conn_t *c, int fd);

/* GIVEN (main.c): one recv() into the connection buffer = lb_read(&c->lb).
 *   >0 bytes appended, 0 peer closed, -1 error (EAGAIN: nothing yet on a
 *   non-blocking socket; EINTR: interrupted). */
ssize_t conn_read(conn_t *c);

/* TODO 3 (conn.c): process EVERY complete request line currently in the buffer:
 *   loop lb_next_line -> exam_handle_line -> send_all.
 *   too long line  -> send "ERR TOOLONG\n" (strlen bytes, no NUL) and return -1
 *   QUIT (close_after) -> send the reply and return -1
 *   send_all fails -> return -1
 * Returns 0 if the connection stays open, -1 if the caller must close it. */
int conn_serve(conn_t *c);

#endif
