/* server.h -- the ONLY server interface header. */
#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include <sys/types.h>
#include "exam.h"
#include "linebuf.h"

typedef struct {
    linebuf_t lb;
    session_t sess;
} conn_t;

extern volatile sig_atomic_t g_stop; /* set by the GIVEN handler in main.c */

/* GIVEN helpers in main.c. */
void conn_init(conn_t *c, int fd);
ssize_t conn_read(conn_t *c);
int set_nonblocking(int fd);
void serve_client(int fd);       /* read loop + close for blocking workers */

/* GIVEN in servers.c: dispatch every complete request already buffered. */
int conn_serve(conn_t *c);

/* Student server models in servers.c. The listener is already bound. */
int run_iterative(int listen_fd); /* TODO 2: accept + serve one at a time */
int run_thread(int listen_fd);    /* TODO 3: thread per connection */
int run_poll(int listen_fd);      /* TODO 4: one-thread poll event loop */
int run_select(int listen_fd);    /* TODO 5: one-thread select event loop */
int run_fork(int listen_fd);      /* TODO 6: process per connection */
int run_epoll(int listen_fd);     /* TODO 7: one-thread epoll event loop */

#endif
