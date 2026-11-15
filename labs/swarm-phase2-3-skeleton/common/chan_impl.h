/* chan_impl.h — how a transport plugs into chan.h (given). Only chan*.c files include this.
 *
 * An "interface" in C: struct chan holds a pointer to a table of functions (its operations).
 * Every transport defines its own struct that starts with a struct chan, and its own
 * operations. chan_send(c, ...) simply calls c->ops->send(c, ...). The Linux kernel does the same
 * for files (struct file_operations): read() works on a disk file, a pipe or a socket.
 *
 *   struct shm_chan {               // in chan_shm.c
 *       struct chan base;           // MUST be the first member
 *       ... your fields ...
 *   };
 *   static const struct chan_ops shm_ops = { .send = shm_send, .recv = shm_recv, .close = shm_close };
 *   ...
 *   struct shm_chan *s = calloc(1, sizeof *s);
 *   s->base.ops = &shm_ops;  s->base.mode = mode;
 *   return &s->base;               // a struct shm_chan * is also a struct chan *
 *
 * and in shm_send:  struct shm_chan *s = (struct shm_chan *)c;
 */
#ifndef CHAN_IMPL_H
#define CHAN_IMPL_H

#include "chan.h"

struct chan_ops {
    int     (*send)(chan_t *c, const void *msg, size_t len);
    ssize_t (*recv)(chan_t *c, void *buf, size_t cap, int timeout_ms);
    void    (*close)(chan_t *c);          /* must free the struct */
    /* optional, Phase 3 (leave them out, i.e. NULL, where they make no sense): */
    void    (*shutdown)(chan_t *c);                              /* chan_shutdown */
    chan_t *(*accept)(chan_t *c, int timeout_ms);                /* chan_accept (a listener) */
    int     (*addr)(chan_t *c, int which, char *host, size_t n); /* chan_addr */
    chan_t *(*peer)(chan_t *c);                                  /* chan_peer (udp) */
};

struct chan {
    const struct chan_ops *ops;
    int mode;                             /* CHAN_SEND, CHAN_RECV or CHAN_BOTH */
};

/* The transports. `name` is the part of the address after the "xxx:" prefix. Each returns NULL
 * on error. */
chan_t *chan_shm_open(const char *name, int mode);      /* chan_shm.c   (Task 1) */
chan_t *chan_fifo_open(const char *name, int mode);     /* chan_fifo.c  (Task 3) */
chan_t *chan_mq_open(const char *name, int mode);       /* chan_mq.c    (Task 3, optional) */
chan_t *chan_file_open(const char *path, int mode);     /* chan_file.c  (Task 4) */
chan_t *chan_tcp_open(const char *hostport, int mode);  /* chan_tcp.c   (Phase 3, Task 1) */
chan_t *chan_tcp_listen(const char *hostport);          /* chan_tcp.c   (Phase 3, Task 1) */
chan_t *chan_udp_open(const char *hostport, int mode);  /* chan_udp.c   (Phase 3, Task 1) */

/* Given helpers for the network transports (chan.c):
 * split "HOST:PORT" (or ":PORT", or "[v6addr]:PORT") into host ("" when there is none) and port;
 * 0 ok, -1 malformed. */
int chan_split_hostport(const char *hostport, char *host, size_t hn, char *port, size_t pn);
/* the address of a socket (getsockname / getpeername) as chan_addr returns it */
int chan_sockaddr(int fd, int which, char *host, size_t n);

/* For transports that need a deadline: now + timeout_ms on the given clock. */
struct timespec;
void chan_deadline(struct timespec *ts, int clock_id, int timeout_ms);

#endif
