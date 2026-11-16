/* chan.h — ONE API for every link of the show (given: declarations only, you write chan/).
 *
 * Every link between two programs is a channel. The address picks what carries the bytes, so the
 * code that uses a channel is the same on one machine (Phase 2) and on a network (Phase 3):
 *
 *   address          phase  carried by                  delivery
 *   ---------------  -----  --------------------------  -----------------------------------------
 *   fifo:PATH        2      named pipes (FIFOs)         every line, in order, both ways
 *   shm:NAME         2      POSIX shared memory         the newest message wins (a slow reader skips)
 *   file:PATH        2      a file                      every message kept, read back from the start
 *   tcp:HOST:PORT    3      a TCP connection            every line, in order, both ways
 *   udp:HOST:PORT    3      UDP datagrams               may be lost, duplicated or reordered
 *
 * PROTOCOL.md, Section 1, has the exact rules of each scheme. In short:
 *   - Messages, not bytes. On fifo: and tcp: a message is one text line: chan_send adds the '\n',
 *     chan_recv removes it and returns one line. A buffer that holds several '\n'-separated lines is
 *     sent in one go and received as several messages. On shm:, file: and udp: a message is any
 *     bytes, delivered whole (a Frame on shm: and file:, one protocol line on udp:).
 *   - Nothing waits forever unless asked: every wait has a timeout.
 *   - The reply rule: on a UDP server (chan_listen("udp::PORT")), chan_send goes to the sender of
 *     the last datagram received.
 *   - chan_close removes what its side created (FIFO files, a shared memory object).
 *   - SWARM_BIND=ADDR (environment): tcp: and udp: sockets bind to ADDR, so each program on one
 *     machine can have its own 127.0.0.x. SWARM_DROP=PCT: udp: drops PCT% of the datagrams it sends.
 */
#ifndef CHAN_H
#define CHAN_H

#include <stddef.h>
#include <sys/types.h>

typedef struct chan chan_t;              /* you define struct chan in chan/ */

enum { CHAN_SEND = 1, CHAN_RECV = 2, CHAN_BOTH = 3 };     /* chan_open modes */
enum { CHAN_LOCAL = 0, CHAN_REMOTE = 1 };                /* chan_addr sides */

/* Results (a successful chan_recv returns the message length, >= 0) */
#define CHAN_CLOSED   (-1)    /* the other side is gone or closed: nothing more will come */
#define CHAN_TIMEOUT  (-2)    /* nothing arrived within timeout_ms                       */
#define CHAN_TOOBIG   (-3)    /* the message did not fit in buf: it was dropped          */

/* Open one end of a channel.
 *   shm:, file:          mode CHAN_SEND (creates it) or CHAN_RECV (opens an existing one)
 *   fifo:, tcp:, udp:    mode CHAN_BOTH: connect to that server (udp: talks to that one address)
 * NULL on error, with errno set (ECONNREFUSED: nobody listens there; ENOENT: no such bus or file). */
chan_t *chan_open(const char *addr, int mode);

/* A server: "fifo:PATH", "tcp:HOST:PORT", "udp:HOST:PORT". An empty HOST ("tcp::PORT") means
 * every address; PORT 0 means any free port (chan_addr tells which). NULL on error. */
chan_t *chan_listen(const char *addr);

/* The next client of a fifo: or tcp: server, as a CHAN_BOTH channel. NULL if none arrived
 * within timeout_ms (-1: no limit). */
chan_t *chan_accept(chan_t *server, int timeout_ms);

/* Send one message. 0, CHAN_CLOSED or CHAN_TOOBIG. Never blocks for long (shm: never waits for a
 * reader). Two threads that send on the same channel must hold a lock of yours around it. */
int chan_send(chan_t *c, const void *msg, size_t len);

/* Receive one message into buf[cap], waiting at most timeout_ms (-1: no limit, 0: do not wait).
 * Returns its length, CHAN_TIMEOUT, CHAN_CLOSED or CHAN_TOOBIG. If the length is < cap,
 * buf[length] is '\0', so a text message is a C string. */
ssize_t chan_recv(chan_t *c, void *buf, size_t cap, int timeout_ms);

/* Where a channel is. side CHAN_LOCAL: this end; CHAN_REMOTE: the other end (on a UDP server, the
 * sender of the last datagram). Writes the numeric host into host[n] (if host is not NULL) and
 * returns the port; returns 0 for fifo:, shm: and file:, and -1 on error. */
int chan_addr(chan_t *c, int side, char *host, size_t n);

/* Close this end and free it. Safe with NULL. */
void chan_close(chan_t *c);

#endif
