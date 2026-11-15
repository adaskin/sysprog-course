/* chan.h — ONE API for every link of the show (given).
 *
 * Every link in the swarm is a channel: you open it, send messages, receive messages, close it.
 * What carries the bytes (a pipe, shared memory, a FIFO, a file, and in Phase 3 a TCP socket) is
 * chosen by the address, not by the code that uses the channel:
 *
 *   address        transport                      who sends -> who receives            built by
 *   -------------  -----------------------------  -----------------------------------  ---------
 *   chan_fd(i, o)  two pipes (or a socket)        center <-> leader: protocol lines    given
 *   "shm:NAME"     the stage bus: shared memory   center -> any number of viewers      Task 1
 *   "fifo:NAME"    a named pipe /tmp/NAME.ctl     operators -> center: commands        Task 3
 *   "mq:NAME"      a POSIX message queue          (the same, if you prefer it)         Task 3
 *   "file:PATH"    a memory-mapped file           center -> recording -> replay        Task 4
 *   "tcp:HOST:PORT" a TCP connection              center <-> captains, viewers, ops    Phase 3, Task 1
 *   "udp:HOST:PORT" UDP datagrams                 captain <-> its performers (radio)   Phase 3, Task 1
 *
 * So the center publishes a frame with  chan_send(bus, &frame, sizeof frame)  whether the bus is
 * shared memory or a file, and a viewer gets one with  chan_recv(src, &frame, sizeof frame, 30)
 * whether it watches a live show or replays a recording. In Phase 3 only the addresses change.
 *
 * What a "message" is depends on the kind of transport:
 *   - line transports (fd, fifo, tcp): one text line; chan_send adds the '\n', chan_recv strips it
 *     and NUL-terminates. A message may contain several '\n'-separated lines (a PLAN with its
 *     keyframes is sent as one message, in one write); the receiver gets them one line at a time.
 *   - message transports (shm, mq, file, udp): any bytes, up to the transport's maximum size,
 *     delivered whole. The stage bus and the recording carry Frames (common/frame.h); a udp
 *     datagram carries one protocol line, without '\n'.
 *
 * Delivery also depends on the transport, and that is the point of choosing one:
 *   - shm is a "newest value wins" bus: chan_send never blocks and never waits for a receiver;
 *     a receiver gets the newest message it has not seen yet, so a slow receiver skips messages.
 *   - fd, fifo, mq, tcp deliver every message, in order, to one receiver.
 *   - file keeps every message; a receiver reads them back in order, from the start.
 *   - udp may lose, duplicate or reorder messages: that is why every radio message carries its beat.
 */
#ifndef CHAN_H
#define CHAN_H

#include <stddef.h>
#include <sys/types.h>

typedef struct chan chan_t;

enum { CHAN_SEND = 1, CHAN_RECV = 2,    /* the side of the link you are on */
       CHAN_BOTH = CHAN_SEND | CHAN_RECV };  /* tcp and udp: a conversation, both directions */

/* chan_recv and chan_send results (a successful chan_recv returns the message length, >= 0) */
#define CHAN_CLOSED   (-1)    /* the other side is gone or closed the channel: nothing more will come */
#define CHAN_TIMEOUT  (-2)    /* nothing arrived within timeout_ms */
#define CHAN_TOOBIG   (-3)    /* the message did not fit: it was dropped (a line: skipped to its '\n') */

/* Open one end of a channel. mode is CHAN_SEND or CHAN_RECV.
 *   shm:   the CHAN_SEND side creates the bus (one sender); CHAN_RECV sides attach to it.
 *   fifo, mq: the CHAN_RECV side creates it (one receiver, the center); CHAN_SEND sides open it
 *          and fail at once (NULL) if nobody is receiving.
 *   file:  CHAN_SEND creates (or truncates) the file; CHAN_RECV opens an existing one.
 *   tcp:   "tcp:HOST:PORT" with CHAN_BOTH connects (NULL if nobody accepts within a few seconds;
 *          then errno tells why: ECONNREFUSED = nobody listens on that port, ETIMEDOUT = no answer).
 *   udp:   "udp:HOST:PORT" with CHAN_BOTH talks to that one address; "udp::PORT" (no host) listens
 *          on PORT (0: any free port, see chan_addr) and receives from anybody (see chan_peer).
 * Returns NULL on error (with a message on stderr, except for "nobody is receiving"). */
chan_t *chan_open(const char *addr, int mode);

/* A line channel over two file descriptors you already have (pipes from fork, stdin/stdout, and
 * in Phase 3 one socket for both). The channel does not own them until chan_close. */
chan_t *chan_fd(int in_fd, int out_fd);

/* Send one message of len bytes. Returns 0, CHAN_CLOSED, or CHAN_TOOBIG.
 * Not thread-safe for the same channel: if several threads send on one channel, they must take a
 * lock around chan_send (the center's wlock, the leader's out_mu). */
int chan_send(chan_t *c, const void *msg, size_t len);

/* Receive one message into buf[cap]; wait at most timeout_ms (-1: no limit, 0: do not wait).
 * Returns the message length (>= 0), CHAN_TIMEOUT, CHAN_CLOSED, or CHAN_TOOBIG.
 * If the length is < cap, buf[length] is set to '\0', so text messages are C strings. */
ssize_t chan_recv(chan_t *c, void *buf, size_t cap, int timeout_ms);

/* Close this end and free it. The side that created a named object (shm bus, fifo, mq) also
 * removes it, so nothing is left behind in /dev/shm or /tmp. Safe to call with NULL. */
void chan_close(chan_t *c);

/* ------------------------------------------------------------------- Phase 3: networks */

/* Accept connections: "tcp::PORT" (every interface) or "tcp:HOST:PORT"; PORT 0 = any free port.
 * chan_accept waits at most timeout_ms (-1: no limit) for one new connection and returns it as a
 * line channel (CHAN_BOTH); NULL on timeout or error. chan_close(listener) stops listening. */
chan_t *chan_listen(const char *addr);
chan_t *chan_accept(chan_t *listener, int timeout_ms);

/* udp "udp::PORT": a channel that SENDS to whoever sent the last message chan_recv returned on c
 * (it shares c's socket; chan_close on it does not close c). Keep one per peer to answer it later,
 * from any thread. NULL if c has no such thing (not udp, or nothing received yet). */
chan_t *chan_peer(chan_t *c);

/* The local or the remote address of a network channel: returns the port and writes the host
 * (numeric, e.g. "127.0.0.1") to host[n] if host != NULL; -1 if c has no address. */
enum { CHAN_LOCAL = 0, CHAN_REMOTE = 1 };
int chan_addr(chan_t *c, int which, char *host, size_t n);

/* Wake up every thread blocked on c: chan_recv returns CHAN_CLOSED, chan_send fails. The channel
 * stays allocated until chan_close. (For a socket: shutdown(2).) Use it to stop a reader thread,
 * or to cut off a peer that went silent. A no-op on transports without it. */
void chan_shutdown(chan_t *c);

/* The address of the control channel of show `name`: "fifo:NAME" or "mq:NAME" (CTL_TRANSPORT). */
#define CTL_TRANSPORT "fifo"
void ctl_addr(const char *name, char *addr, size_t n);

#endif
